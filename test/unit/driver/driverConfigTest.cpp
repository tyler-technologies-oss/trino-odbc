#include <gtest/gtest.h>
#include <map>
#include <string>

#include "../../../src/driver/config/driverConfig.hpp"

TEST(DriverConfigTest, UnboundedVarcharDefaultsToVarchar) {
  DriverConfig config;
  EXPECT_FALSE(config.getUnboundedVarcharAsLong());
  EXPECT_EQ(config.getUnboundedVarcharStr(), "Varchar");
}

TEST(DriverConfigTest, UnboundedVarcharIgnoresCaseAndSpaces) {
  // The DSN dialog saves "Long Varchar". A connection string is easier
  // to write as LongVarchar, in any case.
  for (std::string value : {"Long Varchar", "LongVarchar", "longvarchar"}) {
    DriverConfig config;
    config.setUnboundedVarchar(value);
    EXPECT_TRUE(config.getUnboundedVarcharAsLong()) << value;
    EXPECT_EQ(config.getUnboundedVarcharStr(), "Long Varchar") << value;
  }
}

TEST(DriverConfigTest, UnknownUnboundedVarcharIsVarchar) {
  DriverConfig config;
  config.setUnboundedVarchar("Long Varchar");
  config.setUnboundedVarchar("nonsense");
  EXPECT_FALSE(config.getUnboundedVarcharAsLong());
}

TEST(DriverConfigTest, ConnectionStringOverridesTheDSN) {
  // SQLDriverConnect lowercases connection string keys, and adds the
  // DSN's values under their camelCase names. The connection string
  // has to win.
  std::map<std::string, std::string> kvps = {
      {"unboundedVarchar", "Varchar"}, {"unboundedvarchar", "LongVarchar"}};
  DriverConfig config = driverConfigFromKVPs(kvps);
  EXPECT_TRUE(config.getUnboundedVarcharAsLong());

  std::map<std::string, std::string> saved = driverConfigToKVPs(config);
  EXPECT_EQ(saved.at("unboundedVarchar"), "Long Varchar");
}
