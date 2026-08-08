#include <aho/core/index.hpp>

#include <gtest/gtest.h>

TEST(AHOIndex, IndexedLineWrapsIndexAccess)
{
    aho::IndexedLine<int> line{{10}, {20}};

    EXPECT_EQ(line[0].index, 10);
    EXPECT_EQ(line[1].index, 20);
    EXPECT_EQ(line[2].index, 10);

    line[3].index = 40;

    EXPECT_EQ(line.idx2.index, 40);
}

TEST(AHOIndex, IndexedTriangleWrapsIndexAccess)
{
    aho::IndexedTriangle<int> triangle{{1}, {2}, {3}};

    EXPECT_EQ(triangle[0].index, 1);
    EXPECT_EQ(triangle[1].index, 2);
    EXPECT_EQ(triangle[2].index, 3);
    EXPECT_EQ(triangle[3].index, 1);

    triangle[5].index = 30;

    EXPECT_EQ(triangle.idx3.index, 30);
}
