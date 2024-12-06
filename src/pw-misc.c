#include <glib.h>
#include <graphene.h>
#include <math.h>
#include <stdio.h>
#include "pw-misc.h"

static bool
approx_eq (double rhl, double lhs, double tolerance)
{
  return fabs (rhl - lhs) < tolerance;
}

// A real-cuberoots-only function:
static double
rcbrt (double num)
{
  if (num < 0)
    return -cbrt (-num);
  return cbrt (num);
}

static double
cbezier_formula (double a, double b, double c, double d, double t)
{
  double omt = (1 - t);
  return pow (omt, 3)*a + 3 * pow (omt, 2) * t * b + 3 * omt * pow (t, 2) * c
         + pow (t, 3) * d;
}

graphene_point_t
curve_get_point (graphene_point_t c1, graphene_point_t c2, graphene_point_t c3,
                 graphene_point_t c4, double t)
{
  graphene_point_t ret = { cbezier_formula (c1.x, c2.x, c3.x, c4.x, t),
                           cbezier_formula (c1.y, c2.y, c3.y, c4.y, t) };
  return ret;
}

// returns an array with size 3, NaN if no solution for root
// https://pomax.github.io/bezierinfo/
double *
get_cubic_roots (double pa, double pb, double pc, double pd)
{
  static double result[] = { NAN, NAN, NAN };

  double a = 3*pa - 6*pb + 3*pc;
  double b = (-3)*pa + 3*pb;
  double c = pa;
  double d = (-pa) + 3*pb - 3*pc + pd;

  static const double tol = 0.0001;
  if (approx_eq (d, 0, tol))
    {
      // this is not a cubic curve.
      if (approx_eq (a, 0, tol))
        {
          {
            // in fact, this is not a quadratic curve either.
            if (approx_eq (b, 0, tol))
              {
                // in fact in fact, there are no solutions.
                return result;
              }
            // linear solution
            result[0] = -c / b;
            return result;
          }
          // quadratic solution
          double q = sqrt (b * b - 4 * a * c);
          result[0] = (q - b) / 2 * a;
          result[1] = (-b - q) / 2 * a;
          return result;
        }
    }

  a /= d;
  b /= d;
  c /= d;

  double p = (3 * b - a*a) / 3;
  double p3 = p / 3;
  double q = (2*a*a*a - 9*a*b + 27*c) / 27;
  double q2 = q / 2;
  double discriminant = q2*q2 + p3*p3*p3;

  // 3 potential roots
  if (discriminant < 0)
    {
      double mp3 = -p / 3;
      double r = sqrt (mp3 * mp3 * mp3);
      double t = -q / (2 * r);
      double cosphi = t < -1 ? -1 : (t > 1 ? 1 : t);
      double phi = acos (cosphi);
      double crtr = rcbrt (r);
      double t1 = 2 * crtr;

      result[0] = t1 * cos (phi / 3) - a / 3;
      result[1] = t1 * cos ((phi + 2 * G_PI) / 3) - a / 3;
      result[2] = t1 * cos ((phi + 4 * G_PI) / 3) - a / 3;
      return result;
    }

  // 3 roots, 2 equal
  if (discriminant == 0)
    {
      double u1 = (q2 < 0) ? rcbrt (-q2) : -rcbrt (q2);
      result[0] = 2 * u1 - a / 3;
      result[1] = -u1 - a / 3;
      return result;
    }

  // 1 real 2 complex
  double sd = sqrt (discriminant);
  double u1 = rcbrt (sd - q2);
  double v1 = rcbrt (sd + q2);
  result[0] = u1 - v1 - a / 3;
  return result;
}

static bool
is_valid_bezier_root (double r)
{
  return 0 < r && r < 1;
}

// align curve points along a line, takes an array of 4 points as 1st param
// https://github.com/Pomax/bezierjs/blob/4e767299184ab4079dc8d7c970020103db14f431/src/utils.js#L513
void
align_curve (graphene_point_t *points, graphene_point_t l1,
             graphene_point_t l2)
{
  double angle = -atan2 (l2.y - l1.y, l2.x - l1.x);
  for (int i = 0; i < 4; i++)
    {
      graphene_point_t p = points[i];
      points[i].x = (p.x - l1.x) * cos (angle) - (p.y - l1.y) * sin (angle);
      points[i].y = (p.x - l1.x) * sin (angle) + (p.y - l1.y) * cos (angle);
    }
}

bool
cbezier_line_intersects (graphene_point_t l1, graphene_point_t l2,
                         graphene_point_t c1, graphene_point_t c2,
                         graphene_point_t c3, graphene_point_t c4)
{
  graphene_point_t pts[4] = { c1, c2, c3, c4 };
  align_curve (pts, l1, l2);
  double *roots = get_cubic_roots (pts[0].y, pts[1].y, pts[2].y, pts[3].y);

  for (int i = 0; i < 3; i++)
  {
    if (roots[i] != NAN && is_valid_bezier_root (roots[i]))
      {
        printf ("Intersection detected: %f\n", roots[i]);
        return true;
      }
  }
  return false;
}
