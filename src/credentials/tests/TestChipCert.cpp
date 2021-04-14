/*
 *
 *    Copyright (c) 2020-2021 Project CHIP Authors
 *    Copyright (c) 2019 Google LLC.
 *    Copyright (c) 2013-2017 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *      Unit tests for CHIP certificate functionality.
 *
 */

#include <core/CHIPTLV.h>
#include <credentials/CHIPCert.h>
#include <support/CHIPMem.h>
#include <support/CodeUtils.h>
#include <support/ErrorStr.h>
#include <support/UnitTestRegistration.h>

#include <nlunit-test.h>

#include "CHIPCert_test_vectors.h"

using namespace chip;
using namespace chip::ASN1;
using namespace chip::TLV;
using namespace chip::Credentials;
using namespace chip::TestCerts;

enum
{
    kStandardCertsCount = 3,
    kTestCertBufSize    = 2048, // Size of buffer needed to hold any of the test certificates
                                // (in either CHIP or DER form), or to decode the certificates.
};

static const BitFlags<CertValidateFlags> sIgnoreNotBeforeFlag(CertValidateFlags::kIgnoreNotBefore);
static const BitFlags<CertValidateFlags> sIgnoreNotAfterFlag(CertValidateFlags::kIgnoreNotAfter);

static const BitFlags<CertDecodeFlags> sNullDecodeFlag;
static const BitFlags<CertDecodeFlags> sGenTBSHashFlag(CertDecodeFlags::kGenerateTBSHash);
static const BitFlags<CertDecodeFlags> sTrustAnchorFlag(CertDecodeFlags::kIsTrustAnchor);

static const BitFlags<TestCertLoadFlags> sNullLoadFlag;
static const BitFlags<TestCertLoadFlags> sDerFormFlag(TestCertLoadFlags::kDERForm);
static const BitFlags<TestCertLoadFlags> sSupIsCAFlag(TestCertLoadFlags::kSuppressIsCA);
static const BitFlags<TestCertLoadFlags> sSupKeyUsageFlag(TestCertLoadFlags::kSuppressKeyUsage);
static const BitFlags<TestCertLoadFlags> sSupKeyCertSignFlag(TestCertLoadFlags::kSuppressKeyCertSign);
static const BitFlags<TestCertLoadFlags> sPathLenZeroFlag(TestCertLoadFlags::kSetPathLenConstZero);

static const BitFlags<KeyPurposeFlags> sNullKPFlag;
static const BitFlags<KeyPurposeFlags> sSA(KeyPurposeFlags::kServerAuth);
static const BitFlags<KeyPurposeFlags> sCA(KeyPurposeFlags::kClientAuth);
static const BitFlags<KeyPurposeFlags> sCS(KeyPurposeFlags::kCodeSigning);
static const BitFlags<KeyPurposeFlags> sEP(KeyPurposeFlags::kEmailProtection);
static const BitFlags<KeyPurposeFlags> sTS(KeyPurposeFlags::kTimeStamping);
static const BitFlags<KeyPurposeFlags> sOS(KeyPurposeFlags::kOCSPSigning);
static const BitFlags<KeyPurposeFlags> sSAandCA(sSA, sCA);
static const BitFlags<KeyPurposeFlags> sSAandCS(sSA, sCS);
static const BitFlags<KeyPurposeFlags> sSAandEP(sSA, sEP);
static const BitFlags<KeyPurposeFlags> sSAandTS(sSA, sTS);

static const BitFlags<KeyUsageFlags> sNullKUFlag;
static const BitFlags<KeyUsageFlags> sDS(KeyUsageFlags::kDigitalSignature);
static const BitFlags<KeyUsageFlags> sNR(KeyUsageFlags::kNonRepudiation);
static const BitFlags<KeyUsageFlags> sKE(KeyUsageFlags::kKeyEncipherment);
static const BitFlags<KeyUsageFlags> sDE(KeyUsageFlags::kDataEncipherment);
static const BitFlags<KeyUsageFlags> sKA(KeyUsageFlags::kKeyAgreement);
static const BitFlags<KeyUsageFlags> sKC(KeyUsageFlags::kKeyCertSign);
static const BitFlags<KeyUsageFlags> sCR(KeyUsageFlags::kCRLSign);
static const BitFlags<KeyUsageFlags> sEO(KeyUsageFlags::kEncipherOnly);
static const BitFlags<KeyUsageFlags> sDO(KeyUsageFlags::kDecipherOnly);
static const BitFlags<KeyUsageFlags> sDSandNR(sDS, sNR);
static const BitFlags<KeyUsageFlags> sDSandKE(sDS, sKE);
static const BitFlags<KeyUsageFlags> sDSandDE(sDS, sDE);
static const BitFlags<KeyUsageFlags> sDSandKA(sDS, sKA);
static const BitFlags<KeyUsageFlags> sDSandKC(sDS, sKC);
static const BitFlags<KeyUsageFlags> sDSandCR(sDS, sCR);
static const BitFlags<KeyUsageFlags> sDSandEO(sDS, sEO);
static const BitFlags<KeyUsageFlags> sDSandDO(sDS, sDO);
static const BitFlags<KeyUsageFlags> sKCandDS(sKC, sDS);
static const BitFlags<KeyUsageFlags> sKCandNR(sKC, sNR);
static const BitFlags<KeyUsageFlags> sKCandKE(sKC, sKE);
static const BitFlags<KeyUsageFlags> sKCandDE(sKC, sDE);
static const BitFlags<KeyUsageFlags> sKCandKA(sKC, sKA);
static const BitFlags<KeyUsageFlags> sKCandCR(sKC, sCR);
static const BitFlags<KeyUsageFlags> sKCandEO(sKC, sEO);
static const BitFlags<KeyUsageFlags> sKCandDO(sKC, sDO);

static CHIP_ERROR LoadTestCertSet01(ChipCertificateSet & certSet)
{
    CHIP_ERROR err;

    err = LoadTestCert(certSet, TestCert::kRoot01, sNullLoadFlag, sTrustAnchorFlag);
    SuccessOrExit(err);

    err = LoadTestCert(certSet, TestCert::kICA01, sNullLoadFlag, sGenTBSHashFlag);
    SuccessOrExit(err);

    err = LoadTestCert(certSet, TestCert::kNode01_01, sNullLoadFlag, sGenTBSHashFlag);
    SuccessOrExit(err);

exit:
    return err;
}

static CHIP_ERROR SetEffectiveTime(ValidationContext & validContext, uint16_t year, uint8_t mon, uint8_t day, uint8_t hour = 0,
                                   uint8_t min = 0, uint8_t sec = 0)
{
    ASN1UniversalTime effectiveTime;

    effectiveTime.Year   = year;
    effectiveTime.Month  = mon;
    effectiveTime.Day    = day;
    effectiveTime.Hour   = hour;
    effectiveTime.Minute = min;
    effectiveTime.Second = sec;

    return ASN1ToChipEpochTime(effectiveTime, validContext.mEffectiveTime);
}

static void TestChipCert_ChipToX509(nlTestSuite * inSuite, void * inContext)
{
    CHIP_ERROR err;
    const uint8_t * inCert;
    uint32_t inCertLen;
    const uint8_t * expectedOutCert;
    uint32_t expectedOutCertLen;
    uint8_t outCertBuf[kTestCertBufSize];
    uint32_t outCertLen;

    for (size_t i = 0; i < gNumTestCerts; i++)
    {
        uint8_t certType = gTestCerts[i];

        err = GetTestCert(certType, sNullLoadFlag, inCert, inCertLen);
        NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
        err = GetTestCert(certType, sDerFormFlag, expectedOutCert, expectedOutCertLen);
        NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

        err = ConvertChipCertToX509Cert(inCert, inCertLen, outCertBuf, sizeof(outCertBuf), outCertLen);
        NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
        NL_TEST_ASSERT(inSuite, outCertLen == expectedOutCertLen);
        NL_TEST_ASSERT(inSuite, memcmp(outCertBuf, expectedOutCert, outCertLen) == 0);
    }
}

static void TestChipCert_X509ToChip(nlTestSuite * inSuite, void * inContext)
{
    CHIP_ERROR err;
    const uint8_t * inCert;
    uint32_t inCertLen;
    const uint8_t * expectedOutCert;
    uint32_t expectedOutCertLen;
    uint8_t outCertBuf[kTestCertBufSize];
    uint32_t outCertLen;

    for (size_t i = 0; i < gNumTestCerts; i++)
    {
        uint8_t certType = gTestCerts[i];

        err = GetTestCert(certType, sDerFormFlag, inCert, inCertLen);
        NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
        err = GetTestCert(certType, sNullLoadFlag, expectedOutCert, expectedOutCertLen);
        NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

        err = ConvertX509CertToChipCert(inCert, inCertLen, outCertBuf, sizeof(outCertBuf), outCertLen);
        NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
        NL_TEST_ASSERT(inSuite, outCertLen == expectedOutCertLen);
        NL_TEST_ASSERT(inSuite, memcmp(outCertBuf, expectedOutCert, outCertLen) == 0);
    }
}

static void TestChipCert_CertValidation(nlTestSuite * inSuite, void * inContext)
{
    CHIP_ERROR err;
    ChipCertificateSet certSet;
    ValidationContext validContext;
    enum
    {
        kMaxCertsPerTestCase = 10
    };

    struct ValidationTestCase
    {
        int mSubjectCertIndex;
        uint8_t mValidateFlags;
        uint8_t mRequiredCertType;
        CHIP_ERROR mExpectedResult;
        int mExpectedCertIndex;
        int mExpectedTrustAnchorIndex;
        struct
        {
            uint8_t Type;
            BitFlags<CertDecodeFlags> DecodeFlags;
            BitFlags<TestCertLoadFlags> LoadFlags;
        } InputCerts[kMaxCertsPerTestCase];
    };

    // Short-hand names to make the test cases table more concise.
    enum
    {
        CTNS   = kCertType_NotSpecified,
        CTCA   = kCertType_ICA,
        CTNode = kCertType_Node,
        CTFS   = kCertType_FirmwareSigning,
    };

    // clang-format off
    static const ValidationTestCase sValidationTestCases[] = {
        //            Reqd                                    Exp   Exp                             Cert             Cert
        // Subj Valid Cert                                    Cert  TA     Cert                     Decode           Load
        // Ind  Flags Type    Expected Result                 Index Index  Type                     Flags            Flags
        // ==================================================================================================================================

        // Basic validation of node certificate with different load orders.
        {  2,   0,    CTNS,   CHIP_NO_ERROR,                  2,    0, { { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       } } },
        {  1,   0,    CTNS,   CHIP_NO_ERROR,                  1,    0, { { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       } } },
        {  0,   0,    CTNS,   CHIP_NO_ERROR,                  0,    2, { { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       } } },

        // Basic validation of certificate, which is signed by Root.
        {  0,   0,    CTNS,   CHIP_NO_ERROR,                  0,    1, { { TestCert::kNode01_02, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       } } },
        {  0,   0,    CTNS,   CHIP_NO_ERROR,                  0,    2, { { TestCert::kNode01_02, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       } } },
        {  0,   0,    CTNS,   CHIP_NO_ERROR,                  0,    3, { { TestCert::kNode01_02, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kRoot02,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       } } },

        // Basic validation of node certificates chaining up to another root.
        {  2,   0,    CTNS,   CHIP_NO_ERROR,                  2,    0, { { TestCert::kRoot02,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kICA02,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kNode02_01, sGenTBSHashFlag,  sNullLoadFlag       } } },
        {  1,   0,    CTNS,   CHIP_NO_ERROR,                  1,    0, { { TestCert::kRoot02,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kNode02_01, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA02,     sGenTBSHashFlag,  sNullLoadFlag       } } },
        {  0,   0,    CTNS,   CHIP_NO_ERROR,                  0,    2, { { TestCert::kNode02_01, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA02,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kRoot02,    sTrustAnchorFlag, sNullLoadFlag       } } },
        {  0,   0,    CTNS,   CHIP_NO_ERROR,                  0,    2, { { TestCert::kNode02_02, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA02,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kRoot02,    sTrustAnchorFlag, sNullLoadFlag       } } },
        {  0,   0,    CTNS,   CHIP_NO_ERROR,                  0,    2, { { TestCert::kNode02_03, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA02,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kRoot02,    sTrustAnchorFlag, sNullLoadFlag       } } },
        {  0,   0,    CTNS,   CHIP_NO_ERROR,                  0,    2, { { TestCert::kNode02_04, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA02,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kRoot02,    sTrustAnchorFlag, sNullLoadFlag       } } },

        // Validation with two copies of root certificate, one trusted, one untrusted.
        {  2,   0,    CTNS,   CHIP_NO_ERROR,                  2,    1, { { TestCert::kRoot01,    sNullDecodeFlag,  sNullLoadFlag       },
                                                                         { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       } } },

        // Validation with two trusted certificates.
        {  2,   0,    CTNS,   CHIP_NO_ERROR,                  2,    1, { { TestCert::kRoot02,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       } } },

        // Failure due to missing CA certificate.
        {  1,   0,    CTNS,   CHIP_ERROR_CA_CERT_NOT_FOUND,  -1,   -1, { { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       } } },
        {  2,   0,    CTNS,   CHIP_ERROR_CA_CERT_NOT_FOUND,  -1,   -1, { { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kICA02,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       } } },

        // Failure due to missing root certificate.
        {  1,   0,    CTNS,   CHIP_ERROR_CA_CERT_NOT_FOUND,  -1,   -1, { { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       } } },
        {  1,   0,    CTNS,   CHIP_ERROR_CA_CERT_NOT_FOUND,  -1,   -1, { { TestCert::kRoot02,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       } } },

        // Failure due to lack of TBS hash.
        {  1,   0,    CTNS,   CHIP_ERROR_INVALID_ARGUMENT,   -1,   -1, { { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sNullDecodeFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       } } },

        // Failure due to untrusted root.
        {  1,   0,    CTNS,   CHIP_ERROR_CA_CERT_NOT_FOUND,  -1,   -1, { { TestCert::kRoot01,    sNullDecodeFlag,  sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       } } },

        // Failure due to intermediate cert with isCA flag = false
        {  2,   0,    CTNS,   CHIP_ERROR_CA_CERT_NOT_FOUND,  -1,   -1, { { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sSupIsCAFlag        },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       } } },

        // Failure due to CA cert with no key usage.
        {  2,   0,    CTNS,   CHIP_ERROR_CA_CERT_NOT_FOUND,  -1,   -1, { { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sSupKeyUsageFlag    },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       } } },

        // Failure due to CA cert with no cert sign key usage.
        {  2,   0,    CTNS,   CHIP_ERROR_CA_CERT_NOT_FOUND,  -1,   -1, { { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sSupKeyCertSignFlag },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       } } },

        // Failure due to 3-level deep cert chain and root cert with path constraint == 0
        {  2,   0,    CTNS,   CHIP_ERROR_CA_CERT_NOT_FOUND,  -1,   -1, { { TestCert::kRoot01,    sTrustAnchorFlag, sPathLenZeroFlag    },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       } } },

        // Require a specific certificate type.
        {  2,   0,    CTNode, CHIP_NO_ERROR,                  2,    0, { { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       } } },

        // Failure due to required certificate type not found.
        {  2,   0,    CTCA,   CHIP_ERROR_WRONG_CERT_TYPE,    -1,   -1, { { TestCert::kRoot01,    sTrustAnchorFlag, sNullLoadFlag       },
                                                                         { TestCert::kICA01,     sGenTBSHashFlag,  sNullLoadFlag       },
                                                                         { TestCert::kNode01_01, sGenTBSHashFlag,  sNullLoadFlag       } } },
    };
    // clang-format on
    static const size_t sNumValidationTestCases = sizeof(sValidationTestCases) / sizeof(sValidationTestCases[0]);

    for (unsigned i = 0; i < sNumValidationTestCases; i++)
    {
        ChipCertificateData * resultCert    = nullptr;
        const ValidationTestCase & testCase = sValidationTestCases[i];

        // Initialize the certificate set and load the specified test certificates.
        certSet.Init(kMaxCertsPerTestCase, kTestCertBufSize);
        for (size_t i2 = 0; i2 < kMaxCertsPerTestCase; i2++)
        {
            if (testCase.InputCerts[i2].Type != TestCert::kNone)
            {
                err = LoadTestCert(certSet, testCase.InputCerts[i2].Type, testCase.InputCerts[i2].LoadFlags,
                                   testCase.InputCerts[i2].DecodeFlags);
                NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
            }
        }
        // Make sure the test case is valid.
        NL_TEST_ASSERT(inSuite, testCase.mSubjectCertIndex >= 0 && testCase.mSubjectCertIndex < certSet.GetCertCount());
        if (testCase.mExpectedResult == CHIP_NO_ERROR)
        {
            NL_TEST_ASSERT(inSuite, testCase.mExpectedCertIndex >= 0 && testCase.mExpectedCertIndex < certSet.GetCertCount());
            NL_TEST_ASSERT(inSuite,
                           testCase.mExpectedTrustAnchorIndex >= 0 && testCase.mExpectedTrustAnchorIndex < certSet.GetCertCount());
        }

        // Initialize the validation context.
        validContext.Reset();
        err = SetEffectiveTime(validContext, 2021, 1, 1);
        NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
        validContext.mRequiredKeyUsages.Set(KeyUsageFlags::kDigitalSignature);
        validContext.mRequiredKeyPurposes.Set(KeyPurposeFlags::kServerAuth);
        validContext.mValidateFlags.SetRaw(testCase.mValidateFlags);
        validContext.mRequiredCertType = testCase.mRequiredCertType;

        // Locate the subject DN and key id that will be used as input the FindValidCert() method.
        const ChipDN & subjectDN              = certSet.GetCertSet()[testCase.mSubjectCertIndex].mSubjectDN;
        const CertificateKeyId & subjectKeyId = certSet.GetCertSet()[testCase.mSubjectCertIndex].mSubjectKeyId;

        // Invoke the FindValidCert() method (the method being tested).
        err = certSet.FindValidCert(subjectDN, subjectKeyId, validContext, resultCert);
        NL_TEST_ASSERT(inSuite, err == testCase.mExpectedResult);

        // If the test case is expected to be successful...
        if (err == CHIP_NO_ERROR)
        {
            // Verify that the method found the correct certificate.
            NL_TEST_ASSERT(inSuite, resultCert == &certSet.GetCertSet()[testCase.mExpectedCertIndex]);

            // Verify that the method selected the correct trust anchor.
            NL_TEST_ASSERT(inSuite, validContext.mTrustAnchor == &certSet.GetCertSet()[testCase.mExpectedTrustAnchorIndex]);
        }

        // Clear the certificate set.
        certSet.Release();
    }
}

static void TestChipCert_CertValidTime(nlTestSuite * inSuite, void * inContext)
{
    CHIP_ERROR err;
    ChipCertificateSet certSet;
    ValidationContext validContext;

    certSet.Init(kStandardCertsCount, kTestCertBufSize);

    err = LoadTestCertSet01(certSet);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    validContext.Reset();
    validContext.mRequiredKeyUsages.Set(KeyUsageFlags::kDigitalSignature);
    validContext.mRequiredKeyPurposes.Set(KeyPurposeFlags::kServerAuth);

    // Before certificate validity period.
    err = SetEffectiveTime(validContext, 2020, 1, 3);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    err = certSet.ValidateCert(certSet.GetLastCert(), validContext);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_CERT_NOT_VALID_YET);

    // 1 second before validity period.
    err = SetEffectiveTime(validContext, 2020, 10, 15, 14, 23, 42);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    err = certSet.ValidateCert(certSet.GetLastCert(), validContext);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_CERT_NOT_VALID_YET);

    // 1st second of validity period.
    err = SetEffectiveTime(validContext, 2020, 10, 15, 14, 23, 43);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    err = certSet.ValidateCert(certSet.GetLastCert(), validContext);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    // Validity period.
    err = SetEffectiveTime(validContext, 2022, 02, 23, 12, 30, 01);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    err = certSet.ValidateCert(certSet.GetLastCert(), validContext);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    // Last second of validity period.
    err = SetEffectiveTime(validContext, 2040, 10, 15, 14, 23, 42);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    err = certSet.ValidateCert(certSet.GetLastCert(), validContext);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    // 1 second after end of certificate validity period.
    err = SetEffectiveTime(validContext, 2040, 10, 15, 14, 23, 43);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    err = certSet.ValidateCert(certSet.GetLastCert(), validContext);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_CERT_EXPIRED);

    // After end of certificate validity period.
    err = SetEffectiveTime(validContext, 2042, 4, 25, 0, 0, 0);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    err = certSet.ValidateCert(certSet.GetLastCert(), validContext);
    NL_TEST_ASSERT(inSuite, err == CHIP_ERROR_CERT_EXPIRED);

    // Ignore 'not before' time.
    validContext.mValidateFlags.Set(sIgnoreNotBeforeFlag);
    err = SetEffectiveTime(validContext, 2020, 4, 23, 23, 59, 59);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    err = certSet.ValidateCert(certSet.GetLastCert(), validContext);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    // Ignore 'not after' time.
    validContext.mValidateFlags.Set(sIgnoreNotAfterFlag);
    err = SetEffectiveTime(validContext, 2042, 5, 25, 0, 0, 0);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);
    err = certSet.ValidateCert(certSet.GetLastCert(), validContext);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    certSet.Release();
}

static void TestChipCert_CertUsage(nlTestSuite * inSuite, void * inContext)
{
    CHIP_ERROR err;
    ChipCertificateSet certSet;
    ValidationContext validContext;

    struct UsageTestCase
    {
        uint8_t mCertIndex;
        BitFlags<KeyUsageFlags> mRequiredKeyUsages;
        BitFlags<KeyPurposeFlags> mRequiredKeyPurposes;
        CHIP_ERROR mExpectedResult;
    };

    // clang-format off
    static UsageTestCase sUsageTestCases[] = {
        // Cert Key
        // Ind  Usages       Key Purposes     Expected Result
        // =========================================================================

        // ----- Key Usages for leaf Certificate -----
        {  2,  sDS,          sNullKPFlag,     CHIP_NO_ERROR                        },
        {  2,  sNR,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sKE,          sNullKPFlag,     CHIP_NO_ERROR                        },
        {  2,  sDE,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sKA,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sKC,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sCR,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sEO,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sDO,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sDSandNR,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sDSandKE,     sNullKPFlag,     CHIP_NO_ERROR                        },
        {  2,  sDSandDE,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sDSandKA,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sDSandKC,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sDSandCR,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sDSandEO,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sDSandDO,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },

        // ----- Key Usages for CA Certificate -----
        {  1,  sDS,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sNR,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sKE,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sDE,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sKA,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sKC,          sNullKPFlag,     CHIP_NO_ERROR                        },
        {  1,  sCR,          sNullKPFlag,     CHIP_NO_ERROR                        },
        {  1,  sEO,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sDO,          sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sKCandDS,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sKCandNR,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sKCandKE,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sKCandDE,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sKCandKA,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sKCandCR,     sNullKPFlag,     CHIP_NO_ERROR                        },
        {  1,  sKCandEO,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sKCandDO,     sNullKPFlag,     CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },

        // ----- Key Purposes for leaf Certificate -----
        {  2,  sNullKUFlag,  sSA,             CHIP_NO_ERROR                        },
        {  2,  sNullKUFlag,  sCA,             CHIP_NO_ERROR                        },
        {  2,  sNullKUFlag,  sCS,             CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sNullKUFlag,  sEP,             CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sNullKUFlag,  sTS,             CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sNullKUFlag,  sSAandCA,        CHIP_NO_ERROR                        },
        {  2,  sNullKUFlag,  sSAandCS,        CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sNullKUFlag,  sSAandEP,        CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sNullKUFlag,  sSAandTS,        CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },

        // ----- Key Purposes for CA Certificate -----
        {  1,  sNullKUFlag,  sSA,             CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sNullKUFlag,  sCA,             CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sNullKUFlag,  sCS,             CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sNullKUFlag,  sEP,             CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sNullKUFlag,  sTS,             CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sNullKUFlag,  sSAandCA,        CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sNullKUFlag,  sSAandCS,        CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sNullKUFlag,  sSAandEP,        CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  1,  sNullKUFlag,  sSAandTS,        CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },

        // ----- Combinations -----
        {  2,  sDSandNR,     sSAandCA,        CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sDSandKE,     sSAandCS,        CHIP_ERROR_CERT_USAGE_NOT_ALLOWED    },
        {  2,  sDSandKE,     sSAandCA,        CHIP_NO_ERROR                        },
    };
    // clang-format on
    size_t sNumUsageTestCases = sizeof(sUsageTestCases) / sizeof(sUsageTestCases[0]);

    certSet.Init(kStandardCertsCount, kTestCertBufSize);

    err = LoadTestCertSet01(certSet);
    NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

    for (size_t i = 0; i < sNumUsageTestCases; i++)
    {
        validContext.Reset();
        validContext.mRequiredKeyUsages   = sUsageTestCases[i].mRequiredKeyUsages;
        validContext.mRequiredKeyPurposes = sUsageTestCases[i].mRequiredKeyPurposes;

        err = SetEffectiveTime(validContext, 2020, 10, 16);
        NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

        err = certSet.ValidateCert(&certSet.GetCertSet()[sUsageTestCases[i].mCertIndex], validContext);
        NL_TEST_ASSERT(inSuite, err == sUsageTestCases[i].mExpectedResult);
    }

    certSet.Release();
}

static void TestChipCert_CertType(nlTestSuite * inSuite, void * inContext)
{
    CHIP_ERROR err;
    ChipCertificateSet certSet;

    struct TestCase
    {
        uint8_t Cert;
        uint8_t ExpectedCertType;
    };

    // clang-format off
    static TestCase sTestCases[] = {
        // Cert                        ExpectedCertType
        // =============================================================
        {  TestCert::kRoot01,          kCertType_Root            },
        {  TestCert::kRoot02,          kCertType_Root            },
        {  TestCert::kICA01,           kCertType_ICA             },
        {  TestCert::kICA02,           kCertType_ICA             },
        {  TestCert::kICA01_1,         kCertType_ICA             },
        {  TestCert::kFWSign01,        kCertType_FirmwareSigning },
        {  TestCert::kNode01_01,       kCertType_Node            },
        {  TestCert::kNode01_02,       kCertType_Node            },
        {  TestCert::kNode02_01,       kCertType_Node            },
        {  TestCert::kNode02_02,       kCertType_Node            },
    };
    // clang-format on
    static const size_t sNumTestCases = sizeof(sTestCases) / sizeof(sTestCases[0]);

    for (unsigned i = 0; i < sNumTestCases; i++)
    {
        const TestCase & testCase = sTestCases[i];
        uint8_t certType;

        // Initialize the certificate set and load the test certificate.
        certSet.Init(1, kTestCertBufSize);
        err = LoadTestCert(certSet, testCase.Cert, sNullLoadFlag, sNullDecodeFlag);
        NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

        err = certSet.GetCertSet()->mSubjectDN.GetCertType(certType);
        NL_TEST_ASSERT(inSuite, err == CHIP_NO_ERROR);

        NL_TEST_ASSERT(inSuite, certType == testCase.ExpectedCertType);
    }
}

/**
 *  Set up the test suite.
 */
int TestChipCert_Setup(void * inContext)
{
    CHIP_ERROR error = chip::Platform::MemoryInit();

    if (error != CHIP_NO_ERROR)
    {
        return FAILURE;
    }

    return SUCCESS;
}

/**
 *  Tear down the test suite.
 */
int TestChipCert_Teardown(void * inContext)
{
    chip::Platform::MemoryShutdown();
    return SUCCESS;
}

/**
 *   Test Suite. It lists all the test functions.
 */
// clang-format off
static const nlTest sTests[] = {
    NL_TEST_DEF("Test CHIP Certificate CHIP to X509 Conversion", TestChipCert_ChipToX509),
    NL_TEST_DEF("Test CHIP Certificate X509 to CHIP Conversion", TestChipCert_X509ToChip),
    NL_TEST_DEF("Test CHIP Certificate Validation", TestChipCert_CertValidation),
    NL_TEST_DEF("Test CHIP Certificate Validation time", TestChipCert_CertValidTime),
    NL_TEST_DEF("Test CHIP Certificate Usage", TestChipCert_CertUsage),
    NL_TEST_DEF("Test CHIP Certificate Type", TestChipCert_CertType),
    NL_TEST_SENTINEL()
};
// clang-format on

int TestChipCert(void)
{
    // clang-format off
    nlTestSuite theSuite =
    {
        "Credentials-CHIP-Certs",
        &sTests[0],
        TestChipCert_Setup,
        TestChipCert_Teardown
    };
    // clang-format on
    nlTestRunner(&theSuite, nullptr);
    return (nlTestRunnerStats(&theSuite));
}

CHIP_REGISTER_TEST_SUITE(TestChipCert);
