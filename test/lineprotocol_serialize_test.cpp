#include "gtest/gtest.h"
#include "ngx_http_influxdb_metric.h"

class LineProtocolSerializeTest : public ::testing::Test {
 public:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST_F(LineProtocolSerializeTest, serializeMetricTest) {}
