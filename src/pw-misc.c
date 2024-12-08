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
get_cubic_roots (double pa, double pb, double pc, double pd, double* roots)
{
  roots[0] = NAN;
  roots[1] = NAN;
  roots[2] = NAN;

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
                return roots;
              }
            // linear solution
            roots[0] = -c / b;
            return roots;
          }
          // quadratic solution
          double q = sqrt (b * b - 4 * a * c);
          roots[0] = (q - b) / 2 * a;
          roots[1] = (-b - q) / 2 * a;
          return roots;
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

      roots[0] = t1 * cos (phi / 3) - a / 3;
      roots[1] = t1 * cos ((phi + 2 * G_PI) / 3) - a / 3;
      roots[2] = t1 * cos ((phi + 4 * G_PI) / 3) - a / 3;
      return roots;
    }

  // 3 roots, 2 equal
  if (discriminant == 0)
    {
      double u1 = (q2 < 0) ? rcbrt (-q2) : -rcbrt (q2);
      roots[0] = 2 * u1 - a / 3;
      roots[1] = -u1 - a / 3;
      return roots;
    }

  // 1 real 2 complex
  double sd = sqrt (discriminant);
  double u1 = rcbrt (sd - q2);
  double v1 = rcbrt (sd + q2);
  roots[0] = u1 - v1 - a / 3;
  return roots;
}

static bool
is_valid_bezier_root (double r)
{
  return 0 <= r && r <= 1;
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
  double roots[3];
  get_cubic_roots (pts[0].y, pts[1].y, pts[2].y, pts[3].y, roots);

  for (int i = 0; i < 3; i++)
  {
    if (roots[i] != NAN && (is_valid_bezier_root(roots[i])))
    {
      graphene_point_t r = curve_get_point (c1, c2, c3, c4, roots[i]);
      if ((r.y >= l1.y) && (l2.y >= r.y) && (r.x >= l1.x) && (l2.x >= r.x))
        return true;
    }
  }
  return false;
}


// p'₀= 3(p₁-p₀)
static graphene_point_t
get_point_derivative(graphene_point_t p0, graphene_point_t p1)
{
  return GRAPHENE_POINT_INIT(p1.x - p0.x, p1.y - p0.y);
}

// coefficients must be an array of 3, will contain [a, b, c]
static void
get_cbezier_derivative(double w0, double w1, double w2, double w3, double* coefficients)
{
  double a = 3*(w3 - 3*w2 + 3*w1 - w0);
  double b = 6*(w0 -2*w1 + w2);
  double c = 3*(w1 - w0);

  coefficients[0] = a;
  coefficients[1] = b;
  coefficients[2] = c;
}

// roots must be array of 2, NaN if root doesn't exist
static void
get_quadratic_roots(double a, double b, double c, double* roots)
{
  roots[0] = NAN;
  roots[1] = NAN;

  double discriminant = b*b - 4*a*c;

  if(a==0 || discriminant<0){return;}

  double f = -b/(2*a);
  if(discriminant==0)
  {
    roots[0] = f;
    return;
  }

  // discriminant>0
  double l = sqrt(discriminant)/(2*a);
  roots[0] = f-l;
  roots[1] = f+l;
  return;
}

graphene_rect_t
get_cbezier_bounding_box(graphene_point_t p0, graphene_point_t p1,
                         graphene_point_t p2, graphene_point_t p3)
{
  graphene_point_t v0 = get_point_derivative(p0, p1);
  graphene_point_t v1 = get_point_derivative(p1, p2);
  graphene_point_t v2 = get_point_derivative(p2, p3);

  double x_coefs[3];
  double y_coefs[3];
  get_cbezier_derivative(p0.x, p1.x, p2.x, p3.x, x_coefs);
  get_cbezier_derivative(p0.y, p1.y, p2.y, p3.y, y_coefs);
  double t[4];
  get_quadratic_roots(x_coefs[0], x_coefs[1], x_coefs[2], t);
  get_quadratic_roots(y_coefs[0], y_coefs[1], y_coefs[2], &t[2]);

  gdouble x_min = G_MAXDOUBLE, y_min = G_MAXDOUBLE;
  gdouble x_max = G_MINDOUBLE, y_max = G_MINDOUBLE;
  for(int i=0;i<4;i++)
  {
    if(t[i]!=NAN && is_valid_bezier_root(t[i]))
    {
      graphene_point_t p = curve_get_point(p0, p1, p2, p3, t[i]);
      x_min = MIN(x_min, p.x);
      y_min = MIN(y_min, p.y);
      x_max = MAX(x_max, p.x);
      y_max = MAX(y_max, p.y);
    }
  }

  return GRAPHENE_RECT_INIT(x_min, y_min, x_max-x_min, y_max-y_min);
}

bool rect_contains_rect(graphene_rect_t outter, graphene_rect_t inner)
{
  return
    (inner.origin.x>=outter.origin.x) && ((inner.origin.x+inner.size.width)<(outter.origin.x+outter.size.width)) &&
    (inner.origin.y>=outter.origin.y) && ((inner.origin.y+inner.size.height)<(outter.origin.y+outter.size.height));
}