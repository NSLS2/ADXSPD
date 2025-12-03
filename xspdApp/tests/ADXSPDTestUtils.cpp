#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "ADXSPDAPITest.h"

TEST(ADXSPDAPITest, Test){
    EXPECT_EQ(1,1);
}

// TEST_F(ADXSPDTest, TestGetDataTypeForBitDepth) {
//     EXPECT_EQ(xspdDriver->getDataTypeForBitDepth(1), NDUInt8);
//     EXPECT_EQ(xspdDriver->getDataTypeForBitDepth(6), NDUInt8);
//     EXPECT_EQ(xspdDriver->getDataTypeForBitDepth(12), NDUInt16);
//     EXPECT_EQ(xspdDriver->getDataTypeForBitDepth(24), NDUInt32);
//     EXPECT_THAT([&]() { xspdDriver->getDataTypeForBitDepth(8); }, testing::ThrowsMessage<std::invalid_argument>("Unsupported bit depth"));
// }

// TEST_F(ADXSPDTest, TestGetDeviceId) {
//     EXPECT_EQ(xspdDriver->getDeviceId(), "simdet");
// }

// TEST_F(ADXSPDTest, TestGetDetectorId) {
//     EXPECT_EQ(xspdDriver->getDetectorId(), "xspdsim");
// }

// TEST_F(ADXSPDTest, TestGetLogLevel) {
//     EXPECT_EQ(xspdDriver->getLogLevel(), ADXSPDLogLevel::DEBUG);
// }