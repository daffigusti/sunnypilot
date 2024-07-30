#include "car.h"

namespace {
#define DIM 9
#define EDIM 9
#define MEDIM 9
typedef void (*Hfun)(double *, double *, double *);

double mass;

void set_mass(double x){ mass = x;}

double rotational_inertia;

void set_rotational_inertia(double x){ rotational_inertia = x;}

double center_to_front;

void set_center_to_front(double x){ center_to_front = x;}

double center_to_rear;

void set_center_to_rear(double x){ center_to_rear = x;}

double stiffness_front;

void set_stiffness_front(double x){ stiffness_front = x;}

double stiffness_rear;

void set_stiffness_rear(double x){ stiffness_rear = x;}
const static double MAHA_THRESH_25 = 3.8414588206941227;
const static double MAHA_THRESH_24 = 5.991464547107981;
const static double MAHA_THRESH_30 = 3.8414588206941227;
const static double MAHA_THRESH_26 = 3.8414588206941227;
const static double MAHA_THRESH_27 = 3.8414588206941227;
const static double MAHA_THRESH_29 = 3.8414588206941227;
const static double MAHA_THRESH_28 = 3.8414588206941227;
const static double MAHA_THRESH_31 = 3.8414588206941227;

/******************************************************************************
 *                       Code generated with SymPy 1.12                       *
 *                                                                            *
 *              See http://www.sympy.org/ for more information.               *
 *                                                                            *
 *                         This file is part of 'ekf'                         *
 ******************************************************************************/
void err_fun(double *nom_x, double *delta_x, double *out_7122946553243274237) {
   out_7122946553243274237[0] = delta_x[0] + nom_x[0];
   out_7122946553243274237[1] = delta_x[1] + nom_x[1];
   out_7122946553243274237[2] = delta_x[2] + nom_x[2];
   out_7122946553243274237[3] = delta_x[3] + nom_x[3];
   out_7122946553243274237[4] = delta_x[4] + nom_x[4];
   out_7122946553243274237[5] = delta_x[5] + nom_x[5];
   out_7122946553243274237[6] = delta_x[6] + nom_x[6];
   out_7122946553243274237[7] = delta_x[7] + nom_x[7];
   out_7122946553243274237[8] = delta_x[8] + nom_x[8];
}
void inv_err_fun(double *nom_x, double *true_x, double *out_190832680628474598) {
   out_190832680628474598[0] = -nom_x[0] + true_x[0];
   out_190832680628474598[1] = -nom_x[1] + true_x[1];
   out_190832680628474598[2] = -nom_x[2] + true_x[2];
   out_190832680628474598[3] = -nom_x[3] + true_x[3];
   out_190832680628474598[4] = -nom_x[4] + true_x[4];
   out_190832680628474598[5] = -nom_x[5] + true_x[5];
   out_190832680628474598[6] = -nom_x[6] + true_x[6];
   out_190832680628474598[7] = -nom_x[7] + true_x[7];
   out_190832680628474598[8] = -nom_x[8] + true_x[8];
}
void H_mod_fun(double *state, double *out_1724911219010081507) {
   out_1724911219010081507[0] = 1.0;
   out_1724911219010081507[1] = 0;
   out_1724911219010081507[2] = 0;
   out_1724911219010081507[3] = 0;
   out_1724911219010081507[4] = 0;
   out_1724911219010081507[5] = 0;
   out_1724911219010081507[6] = 0;
   out_1724911219010081507[7] = 0;
   out_1724911219010081507[8] = 0;
   out_1724911219010081507[9] = 0;
   out_1724911219010081507[10] = 1.0;
   out_1724911219010081507[11] = 0;
   out_1724911219010081507[12] = 0;
   out_1724911219010081507[13] = 0;
   out_1724911219010081507[14] = 0;
   out_1724911219010081507[15] = 0;
   out_1724911219010081507[16] = 0;
   out_1724911219010081507[17] = 0;
   out_1724911219010081507[18] = 0;
   out_1724911219010081507[19] = 0;
   out_1724911219010081507[20] = 1.0;
   out_1724911219010081507[21] = 0;
   out_1724911219010081507[22] = 0;
   out_1724911219010081507[23] = 0;
   out_1724911219010081507[24] = 0;
   out_1724911219010081507[25] = 0;
   out_1724911219010081507[26] = 0;
   out_1724911219010081507[27] = 0;
   out_1724911219010081507[28] = 0;
   out_1724911219010081507[29] = 0;
   out_1724911219010081507[30] = 1.0;
   out_1724911219010081507[31] = 0;
   out_1724911219010081507[32] = 0;
   out_1724911219010081507[33] = 0;
   out_1724911219010081507[34] = 0;
   out_1724911219010081507[35] = 0;
   out_1724911219010081507[36] = 0;
   out_1724911219010081507[37] = 0;
   out_1724911219010081507[38] = 0;
   out_1724911219010081507[39] = 0;
   out_1724911219010081507[40] = 1.0;
   out_1724911219010081507[41] = 0;
   out_1724911219010081507[42] = 0;
   out_1724911219010081507[43] = 0;
   out_1724911219010081507[44] = 0;
   out_1724911219010081507[45] = 0;
   out_1724911219010081507[46] = 0;
   out_1724911219010081507[47] = 0;
   out_1724911219010081507[48] = 0;
   out_1724911219010081507[49] = 0;
   out_1724911219010081507[50] = 1.0;
   out_1724911219010081507[51] = 0;
   out_1724911219010081507[52] = 0;
   out_1724911219010081507[53] = 0;
   out_1724911219010081507[54] = 0;
   out_1724911219010081507[55] = 0;
   out_1724911219010081507[56] = 0;
   out_1724911219010081507[57] = 0;
   out_1724911219010081507[58] = 0;
   out_1724911219010081507[59] = 0;
   out_1724911219010081507[60] = 1.0;
   out_1724911219010081507[61] = 0;
   out_1724911219010081507[62] = 0;
   out_1724911219010081507[63] = 0;
   out_1724911219010081507[64] = 0;
   out_1724911219010081507[65] = 0;
   out_1724911219010081507[66] = 0;
   out_1724911219010081507[67] = 0;
   out_1724911219010081507[68] = 0;
   out_1724911219010081507[69] = 0;
   out_1724911219010081507[70] = 1.0;
   out_1724911219010081507[71] = 0;
   out_1724911219010081507[72] = 0;
   out_1724911219010081507[73] = 0;
   out_1724911219010081507[74] = 0;
   out_1724911219010081507[75] = 0;
   out_1724911219010081507[76] = 0;
   out_1724911219010081507[77] = 0;
   out_1724911219010081507[78] = 0;
   out_1724911219010081507[79] = 0;
   out_1724911219010081507[80] = 1.0;
}
void f_fun(double *state, double dt, double *out_4005893408128944833) {
   out_4005893408128944833[0] = state[0];
   out_4005893408128944833[1] = state[1];
   out_4005893408128944833[2] = state[2];
   out_4005893408128944833[3] = state[3];
   out_4005893408128944833[4] = state[4];
   out_4005893408128944833[5] = dt*((-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]))*state[6] - 9.8000000000000007*state[8] + stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*state[1]) + (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*state[4])) + state[5];
   out_4005893408128944833[6] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*state[4])) + state[6];
   out_4005893408128944833[7] = state[7];
   out_4005893408128944833[8] = state[8];
}
void F_fun(double *state, double dt, double *out_9114928829732894524) {
   out_9114928829732894524[0] = 1;
   out_9114928829732894524[1] = 0;
   out_9114928829732894524[2] = 0;
   out_9114928829732894524[3] = 0;
   out_9114928829732894524[4] = 0;
   out_9114928829732894524[5] = 0;
   out_9114928829732894524[6] = 0;
   out_9114928829732894524[7] = 0;
   out_9114928829732894524[8] = 0;
   out_9114928829732894524[9] = 0;
   out_9114928829732894524[10] = 1;
   out_9114928829732894524[11] = 0;
   out_9114928829732894524[12] = 0;
   out_9114928829732894524[13] = 0;
   out_9114928829732894524[14] = 0;
   out_9114928829732894524[15] = 0;
   out_9114928829732894524[16] = 0;
   out_9114928829732894524[17] = 0;
   out_9114928829732894524[18] = 0;
   out_9114928829732894524[19] = 0;
   out_9114928829732894524[20] = 1;
   out_9114928829732894524[21] = 0;
   out_9114928829732894524[22] = 0;
   out_9114928829732894524[23] = 0;
   out_9114928829732894524[24] = 0;
   out_9114928829732894524[25] = 0;
   out_9114928829732894524[26] = 0;
   out_9114928829732894524[27] = 0;
   out_9114928829732894524[28] = 0;
   out_9114928829732894524[29] = 0;
   out_9114928829732894524[30] = 1;
   out_9114928829732894524[31] = 0;
   out_9114928829732894524[32] = 0;
   out_9114928829732894524[33] = 0;
   out_9114928829732894524[34] = 0;
   out_9114928829732894524[35] = 0;
   out_9114928829732894524[36] = 0;
   out_9114928829732894524[37] = 0;
   out_9114928829732894524[38] = 0;
   out_9114928829732894524[39] = 0;
   out_9114928829732894524[40] = 1;
   out_9114928829732894524[41] = 0;
   out_9114928829732894524[42] = 0;
   out_9114928829732894524[43] = 0;
   out_9114928829732894524[44] = 0;
   out_9114928829732894524[45] = dt*(stiffness_front*(-state[2] - state[3] + state[7])/(mass*state[1]) + (-stiffness_front - stiffness_rear)*state[5]/(mass*state[4]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[6]/(mass*state[4]));
   out_9114928829732894524[46] = -dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(mass*pow(state[1], 2));
   out_9114928829732894524[47] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_9114928829732894524[48] = -dt*stiffness_front*state[0]/(mass*state[1]);
   out_9114928829732894524[49] = dt*((-1 - (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*pow(state[4], 2)))*state[6] - (-stiffness_front*state[0] - stiffness_rear*state[0])*state[5]/(mass*pow(state[4], 2)));
   out_9114928829732894524[50] = dt*(-stiffness_front*state[0] - stiffness_rear*state[0])/(mass*state[4]) + 1;
   out_9114928829732894524[51] = dt*(-state[4] + (-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(mass*state[4]));
   out_9114928829732894524[52] = dt*stiffness_front*state[0]/(mass*state[1]);
   out_9114928829732894524[53] = -9.8000000000000007*dt;
   out_9114928829732894524[54] = dt*(center_to_front*stiffness_front*(-state[2] - state[3] + state[7])/(rotational_inertia*state[1]) + (-center_to_front*stiffness_front + center_to_rear*stiffness_rear)*state[5]/(rotational_inertia*state[4]) + (-pow(center_to_front, 2)*stiffness_front - pow(center_to_rear, 2)*stiffness_rear)*state[6]/(rotational_inertia*state[4]));
   out_9114928829732894524[55] = -center_to_front*dt*stiffness_front*(-state[2] - state[3] + state[7])*state[0]/(rotational_inertia*pow(state[1], 2));
   out_9114928829732894524[56] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_9114928829732894524[57] = -center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_9114928829732894524[58] = dt*(-(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])*state[5]/(rotational_inertia*pow(state[4], 2)) - (-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])*state[6]/(rotational_inertia*pow(state[4], 2)));
   out_9114928829732894524[59] = dt*(-center_to_front*stiffness_front*state[0] + center_to_rear*stiffness_rear*state[0])/(rotational_inertia*state[4]);
   out_9114928829732894524[60] = dt*(-pow(center_to_front, 2)*stiffness_front*state[0] - pow(center_to_rear, 2)*stiffness_rear*state[0])/(rotational_inertia*state[4]) + 1;
   out_9114928829732894524[61] = center_to_front*dt*stiffness_front*state[0]/(rotational_inertia*state[1]);
   out_9114928829732894524[62] = 0;
   out_9114928829732894524[63] = 0;
   out_9114928829732894524[64] = 0;
   out_9114928829732894524[65] = 0;
   out_9114928829732894524[66] = 0;
   out_9114928829732894524[67] = 0;
   out_9114928829732894524[68] = 0;
   out_9114928829732894524[69] = 0;
   out_9114928829732894524[70] = 1;
   out_9114928829732894524[71] = 0;
   out_9114928829732894524[72] = 0;
   out_9114928829732894524[73] = 0;
   out_9114928829732894524[74] = 0;
   out_9114928829732894524[75] = 0;
   out_9114928829732894524[76] = 0;
   out_9114928829732894524[77] = 0;
   out_9114928829732894524[78] = 0;
   out_9114928829732894524[79] = 0;
   out_9114928829732894524[80] = 1;
}
void h_25(double *state, double *unused, double *out_3205075429217995583) {
   out_3205075429217995583[0] = state[6];
}
void H_25(double *state, double *unused, double *out_7841977598755789393) {
   out_7841977598755789393[0] = 0;
   out_7841977598755789393[1] = 0;
   out_7841977598755789393[2] = 0;
   out_7841977598755789393[3] = 0;
   out_7841977598755789393[4] = 0;
   out_7841977598755789393[5] = 0;
   out_7841977598755789393[6] = 1;
   out_7841977598755789393[7] = 0;
   out_7841977598755789393[8] = 0;
}
void h_24(double *state, double *unused, double *out_1675954323987673279) {
   out_1675954323987673279[0] = state[4];
   out_1675954323987673279[1] = state[5];
}
void H_24(double *state, double *unused, double *out_7046840876151439087) {
   out_7046840876151439087[0] = 0;
   out_7046840876151439087[1] = 0;
   out_7046840876151439087[2] = 0;
   out_7046840876151439087[3] = 0;
   out_7046840876151439087[4] = 1;
   out_7046840876151439087[5] = 0;
   out_7046840876151439087[6] = 0;
   out_7046840876151439087[7] = 0;
   out_7046840876151439087[8] = 0;
   out_7046840876151439087[9] = 0;
   out_7046840876151439087[10] = 0;
   out_7046840876151439087[11] = 0;
   out_7046840876151439087[12] = 0;
   out_7046840876151439087[13] = 0;
   out_7046840876151439087[14] = 1;
   out_7046840876151439087[15] = 0;
   out_7046840876151439087[16] = 0;
   out_7046840876151439087[17] = 0;
}
void h_30(double *state, double *unused, double *out_432374099036026767) {
   out_432374099036026767[0] = state[4];
}
void H_30(double *state, double *unused, double *out_3688076133462145468) {
   out_3688076133462145468[0] = 0;
   out_3688076133462145468[1] = 0;
   out_3688076133462145468[2] = 0;
   out_3688076133462145468[3] = 0;
   out_3688076133462145468[4] = 1;
   out_3688076133462145468[5] = 0;
   out_3688076133462145468[6] = 0;
   out_3688076133462145468[7] = 0;
   out_3688076133462145468[8] = 0;
}
void h_26(double *state, double *unused, double *out_4569299168624818878) {
   out_4569299168624818878[0] = state[7];
}
void H_26(double *state, double *unused, double *out_4100474279881733169) {
   out_4100474279881733169[0] = 0;
   out_4100474279881733169[1] = 0;
   out_4100474279881733169[2] = 0;
   out_4100474279881733169[3] = 0;
   out_4100474279881733169[4] = 0;
   out_4100474279881733169[5] = 0;
   out_4100474279881733169[6] = 0;
   out_4100474279881733169[7] = 1;
   out_4100474279881733169[8] = 0;
}
void h_27(double *state, double *unused, double *out_6259699472484715268) {
   out_6259699472484715268[0] = state[3];
}
void H_27(double *state, double *unused, double *out_5537875339812124412) {
   out_5537875339812124412[0] = 0;
   out_5537875339812124412[1] = 0;
   out_5537875339812124412[2] = 0;
   out_5537875339812124412[3] = 1;
   out_5537875339812124412[4] = 0;
   out_5537875339812124412[5] = 0;
   out_5537875339812124412[6] = 0;
   out_5537875339812124412[7] = 0;
   out_5537875339812124412[8] = 0;
}
void h_29(double *state, double *unused, double *out_511135753865635668) {
   out_511135753865635668[0] = state[1];
}
void H_29(double *state, double *unused, double *out_3824512612942573379) {
   out_3824512612942573379[0] = 0;
   out_3824512612942573379[1] = 1;
   out_3824512612942573379[2] = 0;
   out_3824512612942573379[3] = 0;
   out_3824512612942573379[4] = 0;
   out_3824512612942573379[5] = 0;
   out_3824512612942573379[6] = 0;
   out_3824512612942573379[7] = 0;
   out_3824512612942573379[8] = 0;
}
void h_28(double *state, double *unused, double *out_6863015357161192785) {
   out_6863015357161192785[0] = state[0];
}
void H_28(double *state, double *unused, double *out_5788142884507899630) {
   out_5788142884507899630[0] = 1;
   out_5788142884507899630[1] = 0;
   out_5788142884507899630[2] = 0;
   out_5788142884507899630[3] = 0;
   out_5788142884507899630[4] = 0;
   out_5788142884507899630[5] = 0;
   out_5788142884507899630[6] = 0;
   out_5788142884507899630[7] = 0;
   out_5788142884507899630[8] = 0;
}
void h_31(double *state, double *unused, double *out_8797057513470062762) {
   out_8797057513470062762[0] = state[8];
}
void H_31(double *state, double *unused, double *out_7872623560632749821) {
   out_7872623560632749821[0] = 0;
   out_7872623560632749821[1] = 0;
   out_7872623560632749821[2] = 0;
   out_7872623560632749821[3] = 0;
   out_7872623560632749821[4] = 0;
   out_7872623560632749821[5] = 0;
   out_7872623560632749821[6] = 0;
   out_7872623560632749821[7] = 0;
   out_7872623560632749821[8] = 1;
}
#include <eigen3/Eigen/Dense>
#include <iostream>

typedef Eigen::Matrix<double, DIM, DIM, Eigen::RowMajor> DDM;
typedef Eigen::Matrix<double, EDIM, EDIM, Eigen::RowMajor> EEM;
typedef Eigen::Matrix<double, DIM, EDIM, Eigen::RowMajor> DEM;

void predict(double *in_x, double *in_P, double *in_Q, double dt) {
  typedef Eigen::Matrix<double, MEDIM, MEDIM, Eigen::RowMajor> RRM;

  double nx[DIM] = {0};
  double in_F[EDIM*EDIM] = {0};

  // functions from sympy
  f_fun(in_x, dt, nx);
  F_fun(in_x, dt, in_F);


  EEM F(in_F);
  EEM P(in_P);
  EEM Q(in_Q);

  RRM F_main = F.topLeftCorner(MEDIM, MEDIM);
  P.topLeftCorner(MEDIM, MEDIM) = (F_main * P.topLeftCorner(MEDIM, MEDIM)) * F_main.transpose();
  P.topRightCorner(MEDIM, EDIM - MEDIM) = F_main * P.topRightCorner(MEDIM, EDIM - MEDIM);
  P.bottomLeftCorner(EDIM - MEDIM, MEDIM) = P.bottomLeftCorner(EDIM - MEDIM, MEDIM) * F_main.transpose();

  P = P + dt*Q;

  // copy out state
  memcpy(in_x, nx, DIM * sizeof(double));
  memcpy(in_P, P.data(), EDIM * EDIM * sizeof(double));
}

// note: extra_args dim only correct when null space projecting
// otherwise 1
template <int ZDIM, int EADIM, bool MAHA_TEST>
void update(double *in_x, double *in_P, Hfun h_fun, Hfun H_fun, Hfun Hea_fun, double *in_z, double *in_R, double *in_ea, double MAHA_THRESHOLD) {
  typedef Eigen::Matrix<double, ZDIM, ZDIM, Eigen::RowMajor> ZZM;
  typedef Eigen::Matrix<double, ZDIM, DIM, Eigen::RowMajor> ZDM;
  typedef Eigen::Matrix<double, Eigen::Dynamic, EDIM, Eigen::RowMajor> XEM;
  //typedef Eigen::Matrix<double, EDIM, ZDIM, Eigen::RowMajor> EZM;
  typedef Eigen::Matrix<double, Eigen::Dynamic, 1> X1M;
  typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> XXM;

  double in_hx[ZDIM] = {0};
  double in_H[ZDIM * DIM] = {0};
  double in_H_mod[EDIM * DIM] = {0};
  double delta_x[EDIM] = {0};
  double x_new[DIM] = {0};


  // state x, P
  Eigen::Matrix<double, ZDIM, 1> z(in_z);
  EEM P(in_P);
  ZZM pre_R(in_R);

  // functions from sympy
  h_fun(in_x, in_ea, in_hx);
  H_fun(in_x, in_ea, in_H);
  ZDM pre_H(in_H);

  // get y (y = z - hx)
  Eigen::Matrix<double, ZDIM, 1> pre_y(in_hx); pre_y = z - pre_y;
  X1M y; XXM H; XXM R;
  if (Hea_fun){
    typedef Eigen::Matrix<double, ZDIM, EADIM, Eigen::RowMajor> ZAM;
    double in_Hea[ZDIM * EADIM] = {0};
    Hea_fun(in_x, in_ea, in_Hea);
    ZAM Hea(in_Hea);
    XXM A = Hea.transpose().fullPivLu().kernel();


    y = A.transpose() * pre_y;
    H = A.transpose() * pre_H;
    R = A.transpose() * pre_R * A;
  } else {
    y = pre_y;
    H = pre_H;
    R = pre_R;
  }
  // get modified H
  H_mod_fun(in_x, in_H_mod);
  DEM H_mod(in_H_mod);
  XEM H_err = H * H_mod;

  // Do mahalobis distance test
  if (MAHA_TEST){
    XXM a = (H_err * P * H_err.transpose() + R).inverse();
    double maha_dist = y.transpose() * a * y;
    if (maha_dist > MAHA_THRESHOLD){
      R = 1.0e16 * R;
    }
  }

  // Outlier resilient weighting
  double weight = 1;//(1.5)/(1 + y.squaredNorm()/R.sum());

  // kalman gains and I_KH
  XXM S = ((H_err * P) * H_err.transpose()) + R/weight;
  XEM KT = S.fullPivLu().solve(H_err * P.transpose());
  //EZM K = KT.transpose(); TODO: WHY DOES THIS NOT COMPILE?
  //EZM K = S.fullPivLu().solve(H_err * P.transpose()).transpose();
  //std::cout << "Here is the matrix rot:\n" << K << std::endl;
  EEM I_KH = Eigen::Matrix<double, EDIM, EDIM>::Identity() - (KT.transpose() * H_err);

  // update state by injecting dx
  Eigen::Matrix<double, EDIM, 1> dx(delta_x);
  dx  = (KT.transpose() * y);
  memcpy(delta_x, dx.data(), EDIM * sizeof(double));
  err_fun(in_x, delta_x, x_new);
  Eigen::Matrix<double, DIM, 1> x(x_new);

  // update cov
  P = ((I_KH * P) * I_KH.transpose()) + ((KT.transpose() * R) * KT);

  // copy out state
  memcpy(in_x, x.data(), DIM * sizeof(double));
  memcpy(in_P, P.data(), EDIM * EDIM * sizeof(double));
  memcpy(in_z, y.data(), y.rows() * sizeof(double));
}




}
extern "C" {

void car_update_25(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_25, H_25, NULL, in_z, in_R, in_ea, MAHA_THRESH_25);
}
void car_update_24(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<2, 3, 0>(in_x, in_P, h_24, H_24, NULL, in_z, in_R, in_ea, MAHA_THRESH_24);
}
void car_update_30(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_30, H_30, NULL, in_z, in_R, in_ea, MAHA_THRESH_30);
}
void car_update_26(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_26, H_26, NULL, in_z, in_R, in_ea, MAHA_THRESH_26);
}
void car_update_27(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_27, H_27, NULL, in_z, in_R, in_ea, MAHA_THRESH_27);
}
void car_update_29(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_29, H_29, NULL, in_z, in_R, in_ea, MAHA_THRESH_29);
}
void car_update_28(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_28, H_28, NULL, in_z, in_R, in_ea, MAHA_THRESH_28);
}
void car_update_31(double *in_x, double *in_P, double *in_z, double *in_R, double *in_ea) {
  update<1, 3, 0>(in_x, in_P, h_31, H_31, NULL, in_z, in_R, in_ea, MAHA_THRESH_31);
}
void car_err_fun(double *nom_x, double *delta_x, double *out_7122946553243274237) {
  err_fun(nom_x, delta_x, out_7122946553243274237);
}
void car_inv_err_fun(double *nom_x, double *true_x, double *out_190832680628474598) {
  inv_err_fun(nom_x, true_x, out_190832680628474598);
}
void car_H_mod_fun(double *state, double *out_1724911219010081507) {
  H_mod_fun(state, out_1724911219010081507);
}
void car_f_fun(double *state, double dt, double *out_4005893408128944833) {
  f_fun(state,  dt, out_4005893408128944833);
}
void car_F_fun(double *state, double dt, double *out_9114928829732894524) {
  F_fun(state,  dt, out_9114928829732894524);
}
void car_h_25(double *state, double *unused, double *out_3205075429217995583) {
  h_25(state, unused, out_3205075429217995583);
}
void car_H_25(double *state, double *unused, double *out_7841977598755789393) {
  H_25(state, unused, out_7841977598755789393);
}
void car_h_24(double *state, double *unused, double *out_1675954323987673279) {
  h_24(state, unused, out_1675954323987673279);
}
void car_H_24(double *state, double *unused, double *out_7046840876151439087) {
  H_24(state, unused, out_7046840876151439087);
}
void car_h_30(double *state, double *unused, double *out_432374099036026767) {
  h_30(state, unused, out_432374099036026767);
}
void car_H_30(double *state, double *unused, double *out_3688076133462145468) {
  H_30(state, unused, out_3688076133462145468);
}
void car_h_26(double *state, double *unused, double *out_4569299168624818878) {
  h_26(state, unused, out_4569299168624818878);
}
void car_H_26(double *state, double *unused, double *out_4100474279881733169) {
  H_26(state, unused, out_4100474279881733169);
}
void car_h_27(double *state, double *unused, double *out_6259699472484715268) {
  h_27(state, unused, out_6259699472484715268);
}
void car_H_27(double *state, double *unused, double *out_5537875339812124412) {
  H_27(state, unused, out_5537875339812124412);
}
void car_h_29(double *state, double *unused, double *out_511135753865635668) {
  h_29(state, unused, out_511135753865635668);
}
void car_H_29(double *state, double *unused, double *out_3824512612942573379) {
  H_29(state, unused, out_3824512612942573379);
}
void car_h_28(double *state, double *unused, double *out_6863015357161192785) {
  h_28(state, unused, out_6863015357161192785);
}
void car_H_28(double *state, double *unused, double *out_5788142884507899630) {
  H_28(state, unused, out_5788142884507899630);
}
void car_h_31(double *state, double *unused, double *out_8797057513470062762) {
  h_31(state, unused, out_8797057513470062762);
}
void car_H_31(double *state, double *unused, double *out_7872623560632749821) {
  H_31(state, unused, out_7872623560632749821);
}
void car_predict(double *in_x, double *in_P, double *in_Q, double dt) {
  predict(in_x, in_P, in_Q, dt);
}
void car_set_mass(double x) {
  set_mass(x);
}
void car_set_rotational_inertia(double x) {
  set_rotational_inertia(x);
}
void car_set_center_to_front(double x) {
  set_center_to_front(x);
}
void car_set_center_to_rear(double x) {
  set_center_to_rear(x);
}
void car_set_stiffness_front(double x) {
  set_stiffness_front(x);
}
void car_set_stiffness_rear(double x) {
  set_stiffness_rear(x);
}
}

const EKF car = {
  .name = "car",
  .kinds = { 25, 24, 30, 26, 27, 29, 28, 31 },
  .feature_kinds = {  },
  .f_fun = car_f_fun,
  .F_fun = car_F_fun,
  .err_fun = car_err_fun,
  .inv_err_fun = car_inv_err_fun,
  .H_mod_fun = car_H_mod_fun,
  .predict = car_predict,
  .hs = {
    { 25, car_h_25 },
    { 24, car_h_24 },
    { 30, car_h_30 },
    { 26, car_h_26 },
    { 27, car_h_27 },
    { 29, car_h_29 },
    { 28, car_h_28 },
    { 31, car_h_31 },
  },
  .Hs = {
    { 25, car_H_25 },
    { 24, car_H_24 },
    { 30, car_H_30 },
    { 26, car_H_26 },
    { 27, car_H_27 },
    { 29, car_H_29 },
    { 28, car_H_28 },
    { 31, car_H_31 },
  },
  .updates = {
    { 25, car_update_25 },
    { 24, car_update_24 },
    { 30, car_update_30 },
    { 26, car_update_26 },
    { 27, car_update_27 },
    { 29, car_update_29 },
    { 28, car_update_28 },
    { 31, car_update_31 },
  },
  .Hes = {
  },
  .sets = {
    { "mass", car_set_mass },
    { "rotational_inertia", car_set_rotational_inertia },
    { "center_to_front", car_set_center_to_front },
    { "center_to_rear", car_set_center_to_rear },
    { "stiffness_front", car_set_stiffness_front },
    { "stiffness_rear", car_set_stiffness_rear },
  },
  .extra_routines = {
  },
};

ekf_lib_init(car)
