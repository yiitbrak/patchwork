#include <graphene.h>

graphene_point_t curve_get_point (graphene_point_t c1, graphene_point_t c2,
                                  graphene_point_t c3, graphene_point_t c4,
                                  double t);

// https://pomax.github.io/bezierinfo/
// returns an array with size 3, -1 means no solution
double *get_cubic_roots (double pa, double pb, double pc, double pd);

void align_curve (graphene_point_t *points, graphene_point_t l1,
                  graphene_point_t l2);

// l1's x must be bigger than l2's
bool cbezier_line_intersects (graphene_point_t l1, graphene_point_t l2,
                              graphene_point_t c1, graphene_point_t c2,
                              graphene_point_t c3, graphene_point_t c4);
