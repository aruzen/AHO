#include <AHO/core/index.hpp>
#include <gtest/gtest.h>
#include <limits>

namespace {
    TEST(IndexedLineTest, IndexAccessWrapsAndReturnsWritableReferences) {
        aho::IndexedLine<unsigned> line{{3}, {7}};
        EXPECT_EQ(&line[0], &line.idx1);
        EXPECT_EQ(&line[1], &line.idx2);
        EXPECT_EQ(line[2].index, 3u);
        EXPECT_EQ(line[std::numeric_limits<std::size_t>::max()].index, 7u);
        line[3].index = 11;
        EXPECT_EQ(line.idx2.index, 11u);
    }
    TEST(IndexedTriangleTest, IndexAccessWrapsAndReturnsWritableReferences) {
        aho::IndexedTriangle<int> triangle{{1}, {4}, {9}};
        EXPECT_EQ(&triangle[0], &triangle.idx1);
        EXPECT_EQ(&triangle[1], &triangle.idx2);
        EXPECT_EQ(&triangle[2], &triangle.idx3);
        EXPECT_EQ(triangle[3].index, 1);
        EXPECT_EQ(triangle[4].index, 4);
        EXPECT_EQ(triangle[5].index, 9);
        triangle[8].index = 12;
        EXPECT_EQ(triangle.idx3.index, 12);
    }
} // namespace
