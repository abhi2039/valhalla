#include "gurka.h"
#include "meili/geometry_helpers.h"
#include <gtest/gtest.h>

using namespace valhalla;
using namespace valhalla::midgard;

namespace {

// Some real world encoded shape data.
const std::string fwd_enc_shape =
    {-24,  -109, -78,  36,   -90,  -4,  -46,  12,   33,  118,  71,   16,   117, 47,  41,   91,   113,
     44,   45,   112,  -125, 1,    80,  -67,  1,    54,  -1,   1,    117,  -31, 2,   -43,  1,    -33,
     1,    21,   -93,  2,    15,   -25, 1,    47,   -97, 1,    86,   -123, 1,   -64, 1,    51,   -110,
     2,    48,   -90,  1,    29,   -84, 1,    67,   -94, 2,    84,   -10,  1,   -86, 1,    82,   -106,
     2,    70,   -32,  1,    64,   -96, 3,    -20,  1,   -126, 2,    112,  -56, 3,   26,   -104, 3,
     15,   -30,  3,    101,  -48,  3,   -87,  1,    -28, 3,    -107, 1,    -62, 1,   113,  118,  -81,
     1,    110,  27,   56,   -106, 1,   71,   -78,  1,   -67,  1,    -122, 2,   -7,  1,    -40,  1,
     -81,  1,    -86,  1,    17,   108, 26,   -84,  1,   -100, 1,    54,   -62, 1,   -111, 2,    -120,
     3,    -127, 2,    -16,  1,    127, -62,  1,    -43, 1,    -32,  1,    -67, 2,   38,   -49,  1,
     -100, 1,    64,   14,   -74,  1,   105,  -100, 2,   -125, 1,    -44,  3,   -81, 1,    -126, 3,
     -93,  1,    -64,  1,    -67,  2,   -82,  3,    -85, 1,    -36,  1,    -75, 1,   48,   -67,  1,
     -128, 1,    -111, 1,    -72,  2,   -59,  2,    -8,  3,    -113, 3,    -40, 3,   -85,  2,    -126,
     2,    -43,  2,    -68,  2,    -17, 1,    -46,  1,   -35,  2,    -4,   2,   42,  -106, 1,    -96,
     1,    48,   -82,  7,    -38,  4,   -102, 2,    -90, 1,    -74,  3,    -36, 1,   -16,  1,    47,
     -38,  2,    -105, 2,    -70,  1,   -37,  1,    -34, 2,    -85,  2,    -44, 1,   117,  -106, 2,
     91,   -42,  2,    -68,  1,    -18, 2,    -126, 1,   -62,  7,    -62,  2,   -4,  2,    -106, 1,
     -2,   3,    -118, 1,    -78,  1,   -118, 1};

//---------------------------------------------------------------------------
// Convert an encoded shape into a vector of points.
//---------------------------------------------------------------------------
void get_shape(const char* enc_shape, size_t size_enc_shape, std::vector<PointLL>& shape_points) {
  midgard::Shape7Decoder<midgard::PointLL> shape(enc_shape, size_enc_shape);
  shape_points.clear();
  while (!shape.empty())
    shape_points.emplace_back(shape.pop());
}

//---------------------------------------------------------------------------
// There's probably a valhalla utility function for this... but a
// quick search didn't find it.
//---------------------------------------------------------------------------
double get_shape_length(const std::vector<PointLL>& shape) {
  double dist = 0.0;

  if (shape.size() > 1) {
    const PointLL* prev_pt = &shape[0];
    for (size_t i = 1; i < shape.size(); i++) {
      const PointLL* curr_pt = &shape[i];
      dist += curr_pt->Distance(*prev_pt);
      prev_pt = curr_pt;
    }
  }

  return dist;
}

//===========================================================================
// Take a given real-world road shape. Create a second shape that is the
// reverse shape-point order of the first.
// Take a handful of "gps points" and project them onto both shapes.
// Show that these fwd/rev projections:
// 1) project to the same location
// 2) that the fwd_percentage_along + rev_percentage_along == 1.0 (within tol)
//===========================================================================
TEST(geometry, fwd_rev_projection_tols) {
  std::vector<PointLL> fwd_shape_points;
  get_shape(fwd_enc_shape.c_str(), fwd_enc_shape.size(), fwd_shape_points);
  std::vector<PointLL> rev_shape_points(fwd_shape_points);
  std::reverse(rev_shape_points.begin(), rev_shape_points.end());
  std::string rev_enc_shape = midgard::encode7(rev_shape_points);

  // This first check ensures that the length of a real-world series of segments
  // is equal when measured forwards and backwards. This is reasonable to expect.
  // The possible issues that might come into play is if our underlying Distance()
  // computation is 1) too approximate, and/or 2) employs some sort of curved
  // earth bias based on the "first" point of each segment.
  double fwd_shape_length = get_shape_length(fwd_shape_points);
  double rev_shape_length = get_shape_length(rev_shape_points);
  EXPECT_TRUE(std::fabs(fwd_shape_length - rev_shape_length) < 1e-5);

  // Project these gps points onto the fwd-shape and the rev-shape.
  // See that they project to the same point and that their projection
  // percentages sum to 1.0.
  const PointLL gps_points[] = {{13.262609100000001, 38.159699600000003},
                                {13.262637200000002, 38.159659599999998},
                                {13.262741800000005, 38.159575799999997},
                                {13.262665099999999, 38.159486100000001},
                                {13.262594400000001, 38.159471100000003},
                                {13.262595700000003, 38.159450400000005}};

  size_t i = 0;
  for (const auto gps_point : gps_points) {
    // meili::helpers::Project() consumes the incoming shape so we have to
    // create it with each iteration.
    midgard::Shape7Decoder<midgard::PointLL> fwd_shape(fwd_enc_shape.c_str(), fwd_enc_shape.size());

    float fwd_sq_distance = -1.f;
    size_t fwd_segment;
    float fwd_percentage_along = -1.f;
    PointLL fwd_proj_point;
    std::tie(fwd_proj_point, fwd_sq_distance, fwd_segment, fwd_percentage_along) =
        valhalla::meili::helpers::Project(gps_point, fwd_shape);

    // meili::helpers::Project() consumes the incoming shape so we have to
    // create it with each iteration.
    midgard::Shape7Decoder<midgard::PointLL> rev_shape(rev_enc_shape.c_str(), rev_enc_shape.size());

    float rev_sq_distance = -1.f;
    size_t rev_segment;
    float rev_percentage_along = -1.f;
    PointLL rev_proj_point;
    std::tie(rev_proj_point, rev_sq_distance, rev_segment, rev_percentage_along) =
        valhalla::meili::helpers::Project(gps_point, rev_shape);

    // The first gps point is a special case! It motivated the float precision
    // logic in valhalla::meili::helpers::Project().
    if (i++ == 0) {
      ASSERT_EQ(fwd_percentage_along, 0.0);
      ASSERT_EQ(rev_percentage_along, 1.0);
    }

    // 6 decimal digits is roughly 0.1 m, a reasonable expectation
    constexpr double tenth_of_meter_in_deg = 0.000001;
    ASSERT_TRUE(std::fabs(fwd_proj_point.lat() - rev_proj_point.lat()) < tenth_of_meter_in_deg);
    ASSERT_TRUE(std::fabs(fwd_proj_point.lng() - rev_proj_point.lng()) < tenth_of_meter_in_deg);

    // wherever we project, the percentages must add up to 1.0 (without tol)
    float total_percent = fwd_percentage_along + rev_percentage_along;
    ASSERT_TRUE(std::fabs(total_percent - 1.0) < 0.0001);
  }
}

} // namespace