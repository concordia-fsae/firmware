/**
 * @file lib_madgwick.c
 * @brief Source file for the madgwick filter library
 */

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_madgwick.h"
#include <math.h>

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define M_PI 3.14159265358979323846f

/******************************************************************************
 *                     P R I V A T E  F U N C T I O N S
 ******************************************************************************/

static void quat_normalize(float* q0, float* q1, float* q2, float* q3) {
    float n2 = (*q0)*(*q0) + (*q1)*(*q1) + (*q2)*(*q2) + (*q3)*(*q3);
    if (n2 < (EPS * EPS)) {
        *q0 = 1.0f; *q1 = *q2 = *q3 = 0.0f;
        return;
    }
    float invn = 1.0f / sqrtf(n2);
    *q0 *= invn; *q1 *= invn; *q2 *= invn; *q3 *= invn;
}

static void euler_from_quat(const lib_madgwick_S* f, lib_madgwick_euler_S* e) {
    const float q0 = f->q0, q1 = f->q1, q2 = f->q2, q3 = f->q3;

    // roll (x-axis rotation)
    float sinr_cosp = 2.0f * (q0*q1 + q2*q3);
    float cosr_cosp = 1.0f - 2.0f * (q1*q1 + q2*q2);
    e->x = atan2f(sinr_cosp, cosr_cosp);

    // pitch (y-axis rotation)
    float sinp = 2.0f * (q0*q2 - q3*q1);
    if (fabsf(sinp) >= 1.0f) {
        e->y = copysignf((float)M_PI / 2.0f, sinp);
    } else {
        e->y = asinf(sinp);
    }

    // yaw (z-a->xis rotation)
    float siny_cosp = 2.0f * (q0*q3 + q1*q2);
    float cosy_cosp = 1.0f - 2.0f * (q2*q2 + q3*q3);
    e->z = atan2f(siny_cosp, cosy_cosp);
}

/******************************************************************************
 *                       P U B L I C  F U N C T I O N S
 ******************************************************************************/

void madgwick_init(lib_madgwick_S* f, float beta) {
    f->q0 = 1.0f; f->q1 = 0.0f; f->q2 = 0.0f; f->q3 = 0.0f;
    f->beta = beta;
}

void madgwick_set_quaternion(lib_madgwick_S* f, float q0, float q1, float q2, float q3) {
    f->q0 = q0; f->q1 = q1; f->q2 = q2; f->q3 = q3;
    quat_normalize(&f->q0, &f->q1, &f->q2, &f->q3);
}

void madgwick_init_quaternion_from_accel(lib_madgwick_S* f, const lib_madgwick_euler_S* a) {
    float ax = a->x;
    float ay = a->y;
    float az = a->z;

    float a2 = ax*ax + ay*ay + az*az;
    if (a2 < (EPS * EPS)) {
        f->q0 = 1.0f; f->q1 = 0.0f; f->q2 = 0.0f; f->q3 = 0.0f;
        return;
    }

    float inva = 1.0f / sqrtf(a2);
    ax *= inva; ay *= inva; az *= inva;

    // Shortest-arc rotation from +Z to measured accel direction.
    float w = 1.0f + az;
    float q0, q1, q2, q3;
    if (w < EPS) {
        // 180 deg rotation around X (any axis orthogonal to Z is valid).
        q0 = 0.0f; q1 = 1.0f; q2 = 0.0f; q3 = 0.0f;
    } else {
        q0 = w;
        q1 = -ay;
        q2 = ax;
        q3 = 0.0f;
    }
    quat_normalize(&q0, &q1, &q2, &q3);
    f->q0 = q0; f->q1 = q1; f->q2 = q2; f->q3 = q3;
}

void madgwick_get_euler_rad(const lib_madgwick_S* f, lib_madgwick_euler_S* e) {
    euler_from_quat(f, e);
}

void madgwick_get_euler_deg(const lib_madgwick_S* f, lib_madgwick_euler_S* e) {
    madgwick_get_euler_rad(f, e);
    madgwick_euler_rad_to_deg(e);
}

void madgwick_euler_deg_to_rad(lib_madgwick_euler_S* e)
{
    const float k = (float)M_PI / 180.0f;
    e->x *= k;
    e->y *= k;
    e->z *= k;
}

void madgwick_euler_rad_to_deg(lib_madgwick_euler_S* e)
{
    const float k = 180.0f / (float)M_PI;
    e->x *= k;
    e->y *= k;
    e->z *= k;
}

void madgwick_update_imu(lib_madgwick_S* f,
                         const lib_madgwick_euler_S* g,
                         const lib_madgwick_euler_S* a,
                         float dt)
{
    lib_madgwick_euler_S tmpA = {0};
    lib_madgwick_euler_S tmpG = *g;
    float q0 = f->q0, q1 = f->q1, q2 = f->q2, q3 = f->q3;
    const float beta = f->beta;
    madgwick_euler_deg_to_rad(&tmpG);

    // Normalize accelerometer measurement
    float a2 = a->x*a->x + a->y*a->y + a->z*a->z;
    if (a2 < (EPS * EPS)) {
        // If accel is invalid, integrate g->yro only.
        float qDot0 = 0.5f * (-q1*tmpG.x - q2*tmpG.y - q3*tmpG.z);
        float qDot1 = 0.5f * ( q0*tmpG.x + q2*tmpG.z - q3*tmpG.y);
        float qDot2 = 0.5f * ( q0*tmpG.y - q1*tmpG.z + q3*tmpG.x);
        float qDot3 = 0.5f * ( q0*tmpG.z + q1*tmpG.y - q2*tmpG.x);

        q0 += qDot0 * dt; q1 += qDot1 * dt; q2 += qDot2 * dt; q3 += qDot3 * dt;
        quat_normalize(&q0, &q1, &q2, &q3);
        f->q0 = q0; f->q1 = q1; f->q2 = q2; f->q3 = q3;
        return;
    }

    float inva = 1.0f / sqrtf(a2);
    tmpA.x = a->x * inva; tmpA.y = a->y * inva; tmpA.z = a->z * inva;

    // Rate of change of quaternion from g->yro
    float qDot0 = 0.5f * (-q1*tmpG.x - q2*tmpG.y - q3*tmpG.z);
    float qDot1 = 0.5f * ( q0*tmpG.x + q2*tmpG.z - q3*tmpG.y);
    float qDot2 = 0.5f * ( q0*tmpG.y - q1*tmpG.z + q3*tmpG.x);
    float qDot3 = 0.5f * ( q0*tmpG.z + q1*tmpG.y - q2*tmpG.x);

    // Gradient descent corrective step (IMU-only)
    // Objective: align estimated gravity direction with measured accel direction.
    float f1 = 2.0f*(q1*q3 - q0*q2) - tmpA.x;
    float f2 = 2.0f*(q0*q1 + q2*q3) - tmpA.y;
    float f3 = 2.0f*(0.5f - q1*q1 - q2*q2) - tmpA.z;

    // Jacobian transpose * f (s0..s3)
    float s0 = -2.0f*q2*f1 + 2.0f*q1*f2;
    float s1 =  2.0f*q3*f1 + 2.0f*q0*f2 - 4.0f*q1*f3;
    float s2 = -2.0f*q0*f1 + 2.0f*q3*f2 - 4.0f*q2*f3;
    float s3 =  2.0f*q1*f1 + 2.0f*q2*f2;

    // Normalize step magnitude
    float s2norm = s0*s0 + s1*s1 + s2*s2 + s3*s3;
    if (s2norm > (EPS * EPS)) {
        float invs = 1.0f / sqrtf(s2norm);
        s0 *= invs; s1 *= invs; s2 *= invs; s3 *= invs;

        // Apply feedback
        qDot0 -= beta * s0;
        qDot1 -= beta * s1;
        qDot2 -= beta * s2;
        qDot3 -= beta * s3;
    }

    // Integrate to yield quaternion
    q0 += qDot0 * dt; q1 += qDot1 * dt; q2 += qDot2 * dt; q3 += qDot3 * dt;
    quat_normalize(&q0, &q1, &q2, &q3);

    f->q0 = q0; f->q1 = q1; f->q2 = q2; f->q3 = q3;
}

void madgwick_update_mag(lib_madgwick_S* f,
                         const lib_madgwick_euler_S* g,
                         const lib_madgwick_euler_S* a,
                         const lib_madgwick_euler_S* m,
                         float dt)
{
    lib_madgwick_euler_S tmpA = {0};
    lib_madgwick_euler_S tmpM = {0};
    lib_madgwick_euler_S tmpG = *g;

    // If magnetometer is invalid, fall back to IMU update.
    float m2 = m->x*m->x + m->y*m->y + m->z*m->z;
    if (m2 < (EPS * EPS)) {
        madgwick_update_imu(f, g, a, dt);
        return;
    }
    madgwick_euler_deg_to_rad(&tmpG);

    float q0 = f->q0, q1 = f->q1, q2 = f->q2, q3 = f->q3;
    const float beta = f->beta;

    // Normalize accelerometer
    float a2 = a->x*a->x + a->y*a->y + a->z*a->z;
    if (a2 < (EPS * EPS)) {
        // g->yro-only integrate if accel invalid
        float qDot0 = 0.5f * (-q1*tmpG.x - q2*tmpG.y - q3*tmpG.z);
        float qDot1 = 0.5f * ( q0*tmpG.x + q2*tmpG.z - q3*tmpG.y);
        float qDot2 = 0.5f * ( q0*tmpG.y - q1*tmpG.z + q3*tmpG.x);
        float qDot3 = 0.5f * ( q0*tmpG.z + q1*tmpG.y - q2*tmpG.x);

        q0 += qDot0 * dt; q1 += qDot1 * dt; q2 += qDot2 * dt; q3 += qDot3 * dt;
        quat_normalize(&q0, &q1, &q2, &q3);
        f->q0 = q0; f->q1 = q1; f->q2 = q2; f->q3 = q3;
        return;
    }
    float inva = 1.0f / sqrtf(a2);
    tmpA.x = a->x * inva; tmpA.y = a->y * inva; tmpA.z = a->z * inva;

    // Normalize magnetometer
    float invm = 1.0f / sqrtf(m2);
    tmpM.x = m->x * invm; tmpM.y = m->y * invm; tmpM.z = m->z * invm;

    // Rate of change from g->yro
    float qDot0 = 0.5f * (-q1*tmpG.x - q2*tmpG.y - q3*tmpG.z);
    float qDot1 = 0.5f * ( q0*tmpG.x + q2*tmpG.z - q3*tmpG.y);
    float qDot2 = 0.5f * ( q0*tmpG.y - q1*tmpG.z + q3*tmpG.x);
    float qDot3 = 0.5f * ( q0*tmpG.z + q1*tmpG.y - q2*tmpG.x);

    // Reference direction of Earth's magnetic field
    // Compute h = q * m * q_conj
    float hx = 2.0f*tmpM.x*(0.5f - q2*q2 - q3*q3) + 2.0f*tmpM.y*(q1*q2 - q0*q3) + 2.0f*tmpM.z*(q1*q3 + q0*q2);
    float hy = 2.0f*tmpM.x*(q1*q2 + q0*q3) + 2.0f*tmpM.y*(0.5f - q1*q1 - q3*q3) + 2.0f*tmpM.z*(q2*q3 - q0*q1);
    float hz = 2.0f*tmpM.x*(q1*q3 - q0*q2) + 2.0f*tmpM.y*(q2*q3 + q0*q1) + 2.0f*tmpM.z*(0.5f - q1*q1 - q2*q2);

    // b = [|h_xy|, 0, h_z]
    float bx = sqrtf(hx*hx + hy*hy);
    float bz = hz;

    // Objective function (gravity + magnetic)
    float f1 = 2.0f*(q1*q3 - q0*q2) - tmpA.x;
    float f2 = 2.0f*(q0*q1 + q2*q3) - tmpA.y;
    float f3 = 2.0f*(0.5f - q1*q1 - q2*q2) - tmpA.z;

    float f4 = 2.0f*bx*(0.5f - q2*q2 - q3*q3) + 2.0f*bz*(q1*q3 - q0*q2) - tmpM.x;
    float f5 = 2.0f*bx*(q1*q2 - q0*q3) + 2.0f*bz*(q0*q1 + q2*q3) - tmpM.y;
    float f6 = 2.0f*bx*(q0*q2 + q1*q3) + 2.0f*bz*(0.5f - q1*q1 - q2*q2) - tmpM.z;

    // Gradient (Jacobian^T * f), expanded for speed
    float s0 =
        -2.0f*q2*f1 + 2.0f*q1*f2
        -2.0f*bz*q2*f4 + (-2.0f*bx*q3 + 2.0f*bz*q1)*f5 + 2.0f*bx*q2*f6;

    float s1 =
         2.0f*q3*f1 + 2.0f*q0*f2 - 4.0f*q1*f3
        +2.0f*bz*q3*f4 + ( 2.0f*bx*q2 + 2.0f*bz*q0)*f5 + (2.0f*bx*q3 - 4.0f*bz*q1)*f6;

    float s2 =
        -2.0f*q0*f1 + 2.0f*q3*f2 - 4.0f*q2*f3
        +(-4.0f*bx*q2 - 2.0f*bz*q0)*f4 + (2.0f*bx*q1 + 2.0f*bz*q3)*f5 + (2.0f*bx*q0 - 4.0f*bz*q2)*f6;

    float s3 =
         2.0f*q1*f1 + 2.0f*q2*f2
        +(-4.0f*bx*q3 + 2.0f*bz*q1)*f4 + (-2.0f*bx*q0 + 2.0f*bz*q2)*f5 + 2.0f*bx*q1*f6;

    float s2norm = s0*s0 + s1*s1 + s2*s2 + s3*s3;
    if (s2norm > (EPS * EPS)) {
        float invs = 1.0f / sqrtf(s2norm);
        s0 *= invs; s1 *= invs; s2 *= invs; s3 *= invs;

        qDot0 -= beta * s0;
        qDot1 -= beta * s1;
        qDot2 -= beta * s2;
        qDot3 -= beta * s3;
    }

    q0 += qDot0 * dt; q1 += qDot1 * dt; q2 += qDot2 * dt; q3 += qDot3 * dt;
    quat_normalize(&q0, &q1, &q2, &q3);

    f->q0 = q0; f->q1 = q1; f->q2 = q2; f->q3 = q3;
}
