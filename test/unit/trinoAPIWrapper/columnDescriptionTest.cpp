#include <gtest/gtest.h>
#include <string>

#include "../../../src/trinoAPIWrapper/columnDescription.hpp"

TEST(ColumnJsonFromTypeNameTest, SimpleType) {
  ColumnDescription column(columnJsonFromTypeName("custkey", "bigint"));
  EXPECT_EQ(column.getName(), "custkey");
  EXPECT_EQ(column.getType(), "bigint");
  EXPECT_EQ(column.getRawType(), "bigint");
  EXPECT_TRUE(column.getTypeArguments().empty());
}

TEST(ColumnJsonFromTypeNameTest, BoundedVarchar) {
  ColumnDescription column(columnJsonFromTypeName("name", "varchar(25)"));
  EXPECT_EQ(column.getRawType(), "varchar");
  EXPECT_EQ(column.getTypeArguments()[0]["value"], 25);
}

TEST(ColumnJsonFromTypeNameTest, UnboundedVarcharGetsTrinoMaximumLength) {
  ColumnDescription column(columnJsonFromTypeName("name", "varchar"));
  EXPECT_EQ(column.getRawType(), "varchar");
  EXPECT_EQ(column.getTypeArguments()[0]["value"], 2147483647);
}

TEST(ColumnJsonFromTypeNameTest, DecimalPrecisionAndScale) {
  ColumnDescription column(columnJsonFromTypeName("cost", "decimal(38, 9)"));
  EXPECT_EQ(column.getRawType(), "decimal");
  EXPECT_EQ(column.getTypeArguments()[0]["value"], 38);
  EXPECT_EQ(column.getTypeArguments()[1]["value"], 9);
}

TEST(ColumnJsonFromTypeNameTest, ArgumentsInsideTheTypeName) {
  ColumnDescription column(
      columnJsonFromTypeName("ts", "timestamp(3) with time zone"));
  EXPECT_EQ(column.getRawType(), "timestamp with time zone");
  EXPECT_EQ(column.getTypeArguments()[0]["value"], 3);
}

TEST(ColumnJsonFromTypeNameTest, NestedTypeArgumentsAreLeftOut) {
  ColumnDescription column(
      columnJsonFromTypeName("prices", "array(decimal(10,2))"));
  EXPECT_EQ(column.getRawType(), "array");
  EXPECT_TRUE(column.getTypeArguments().empty());
}
