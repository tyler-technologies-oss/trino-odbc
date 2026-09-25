#include <windows.h>

#include <gtest/gtest.h>
#include <sql.h>
#include <sqlext.h>

#include "../../../src/driver/handles/descriptorHandle.hpp"

TEST(DescriptorClearColumnMetadataTest, KeepsBindings) {
  Descriptor descriptor;
  SQLBIGINT buffer = 0;
  SQLLEN indicator = 0;
  DescriptorField field;
  field.bufferCDataType      = SQL_C_SBIGINT;
  field.bufferPtr            = &buffer;
  field.bufferLength         = sizeof(buffer);
  field.bufferStrLenOrIndPtr = &indicator;
  field.columnName           = "Column Name";
  field.odbcDataType         = SQL_VARCHAR;
  field.precision            = 38;
  descriptor.setField(1, field);

  descriptor.clearColumnMetadata();

  DescriptorField clearedField = descriptor.getField(1);
  EXPECT_EQ(clearedField.bufferCDataType, SQL_C_SBIGINT);
  EXPECT_EQ(clearedField.bufferPtr, &buffer);
  EXPECT_EQ(clearedField.bufferLength, static_cast<SQLLEN>(sizeof(buffer)));
  EXPECT_EQ(clearedField.bufferStrLenOrIndPtr, &indicator);
  EXPECT_EQ(clearedField.columnName, "");
  EXPECT_EQ(clearedField.odbcDataType, SQL_UNKNOWN_TYPE);
  EXPECT_EQ(clearedField.precision, 0);
}
