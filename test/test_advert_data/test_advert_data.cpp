#include <gtest/gtest.h>

#include <cstring>

#include <MeshCore.h>
#include <helpers/AdvertDataHelpers.h>

TEST(AdvertDataBuilder, EncodesE6CoordinatesAndName) {
  uint8_t data[MAX_ADVERT_DATA_SIZE];
  auto builder =
      AdvertDataBuilder::fromE6(ADV_TYPE_REPEATER, "TEST-ABCD", 52123456, 4567890);

  const uint8_t len = builder.encodeTo(data);

  EXPECT_EQ(1 + 8 + 9, len);
  EXPECT_EQ(ADV_TYPE_REPEATER | ADV_LATLON_MASK | ADV_NAME_MASK, data[0]);

  int32_t lat = 0;
  int32_t lon = 0;
  std::memcpy(&lat, &data[1], 4);
  std::memcpy(&lon, &data[5], 4);
  EXPECT_EQ(52123456, lat);
  EXPECT_EQ(4567890, lon);
  EXPECT_STREQ("TEST-ABCD", reinterpret_cast<const char*>(&data[9]));
}

TEST(AdvertDataBuilder, TruncatesLongNameToAdvertLimit) {
  uint8_t data[MAX_ADVERT_DATA_SIZE];
  auto builder = AdvertDataBuilder::fromE6(
      ADV_TYPE_REPEATER, "0123456789012345678901234567890", 0, 0);

  const uint8_t len = builder.encodeTo(data);

  EXPECT_EQ(MAX_ADVERT_DATA_SIZE, len);
  EXPECT_EQ(ADV_TYPE_REPEATER | ADV_LATLON_MASK | ADV_NAME_MASK, data[0]);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
