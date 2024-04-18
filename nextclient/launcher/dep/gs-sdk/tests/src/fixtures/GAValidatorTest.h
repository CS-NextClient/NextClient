#pragma once

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class GAValidatorTest : public ::testing::Test
{
protected:
    virtual void SetUp();
    virtual void TearDown();
};
