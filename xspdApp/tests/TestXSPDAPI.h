#ifndef TEST_XSPDAPI_H
#define TEST_XSPDAPI_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>

#include "MockXSPDAPI.h"
#include "cpr/cpr.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

class TestXSPDAPI : public ::testing::Test {
   protected:
    void SetUp() override { mapi = new StrictMock<MockXSPDAPI>(); }

    void TearDown() override { delete mapi; }

    StrictMock<MockXSPDAPI>* mapi;
};

#endif  // TEST_XSPDAPI_H
