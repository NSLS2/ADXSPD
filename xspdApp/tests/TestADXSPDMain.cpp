/**
 * ADXSPDTestMain.cpp
 *
 * Unit tests for ADXSPD areaDetector driver.
 *
 * Author: Jakub Wlodek
 *
 * Copyright (c): Brookhaven National Laboratory 2025
 */
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Main function to run tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::InitGoogleMock(&argc, argv);

    return RUN_ALL_TESTS();
}
