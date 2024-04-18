//
// GA-SDK-CPP
// Copyright 2015 GameAnalytics. All rights reserved.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>


#include <GAHTTPApi.h>
#include <GALogger.h>
#include <GAUtilities.h>
#include "GADevice.h"
#include "GAState.h"
#include "GAStore.h"
#include "GADevice.h"


 TEST(GATests, testInitialize)
 {
     gameanalytics::logging::GALogger::setInfoLog(true);
     gameanalytics::logging::GALogger::i("Info logging enabled");

     gameanalytics::logging::GALogger::setVerboseInfoLog(true);
     gameanalytics::logging::GALogger::i("Verbose logging enabled");

     gameanalytics::state::GAState::setKeys("bd624ee6f8e6efb32a054f8d7ba11618", "7f5c3f682cbd217841efba92e92ffb1b3b6612bc");

     ASSERT_TRUE(gameanalytics::store::GAStore::ensureDatabase(false, "bd624ee6f8e6efb32a054f8d7ba11618"));

     gameanalytics::state::GAState::internalInitialize();
 }

// TEST(GATests, testCompress)
// {
//     std::string data = "Hello world!";
//
//     std::string compressedData = gameanalytics::utilities::GAUtilities::gzipCompress(data);
//
//     namespace bio = boost::iostreams;
//
//     {
//         std::stringstream compressed(compressedData);
//         std::stringstream decompressed;
//
//         bio::filtering_streambuf<bio::input> out;
//         out.push(bio::gzip_decompressor());
//         out.push(compressed);
//         bio::copy(out, decompressed);
//
//         ASSERT_STREQ("Hello world!", decompressed.str().c_str());
//     }
// }
