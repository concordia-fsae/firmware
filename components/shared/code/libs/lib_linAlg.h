/**
 * @file lib_linAlg.h
 * @brief Header file for linear algebra library
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_utility.h"
#include <math.h>

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define EPS 1.0e-2f

#define LIB_LINALG_INST_CVEC(name) linAlg_##name##_cvec_S
#define LIB_LINALG_INST_ROW(name)  linAlg_##name##_rvec_S
#define LIB_LINALG_INST_RMAT(name) linAlg_##name##_rmat_S

/* Vector helpers (structs with elemCol / elemRow) */
#define COLS(vec)  COUNTOF((vec)->elemCol)
#define ROWS(vec)  COUNTOF((vec)->elemRow)

/* Matrix helpers (structs with rows[n][m]) */
#define MROWS(mat) COUNTOF((mat)->rows)
#define MCOLS(mat) COUNTOF((mat)->rows[0])

/******************************************************************************
 *                           T Y P E  D E F S
 ******************************************************************************/

/* Define an N x M row-matrix and matching vectors */
#define LIB_LINALG_DEFINE_NM(name, type, n, m) \
    typedef struct { \
        type elemCol[n]; \
    } LIB_LINALG_INST_CVEC(name); \
    typedef struct { \
        type elemRow[m]; \
    } LIB_LINALG_INST_ROW(name); \
    typedef struct { \
        type rows[n][m]; \
    } LIB_LINALG_INST_RMAT(name)

#define LIB_LINALG_DEFINE_N(name, type, n) \
    LIB_LINALG_DEFINE_NM(name, type, n, n)

/******************************************************************************
 *                          V E C T O R  O P S
 ******************************************************************************/

#define LIB_LINALG_CLEAR_CVEC(a) \
    do { \
        for (uint8_t _i = 0U; _i < COLS(a); _i++) { \
            (a)->elemCol[_i] = 0; \
        } \
    } while (0)

#define LIB_LINALG_SUM_CVEC(a, b, out) \
    _Static_assert(COLS(a) == COLS(b), "Vector size mismatch"); \
    do { \
        for (uint8_t _i = 0U; _i < COLS(a); _i++) { \
            (out)->elemCol[_i] = (a)->elemCol[_i] + (b)->elemCol[_i]; \
        } \
    } while (0)

#define LIB_LINALG_MUL_CVECSCALAR(a, c, out) \
    do { \
        for (uint8_t _i = 0U; _i < COLS(a); _i++) { \
            (out)->elemCol[_i] = (a)->elemCol[_i] * (c); \
        } \
    } while (0)

#define LIB_LINALG_MUL_RVECCVEC(row, col, out) \
    _Static_assert(ROWS(row) == COLS(col), "Vector size mismatch"); \
    do { \
        for (uint8_t _i = 0U; _i < ROWS(row); _i++) { \
            *(out) += (row)->elemRow[_i] * (col)->elemCol[_i]; \
        } \
    } while (0)

#define LIB_LINALG_MUL_RVECCVEC_SET(row, col, out) \
    _Static_assert(ROWS(row) == COLS(col), "Vector size mismatch"); \
    do { \
        *(out) = 0; \
        LIB_LINALG_MUL_RVECCVEC_SET(row, col, out); \
    } while (0)

/******************************************************************************
 *                          N O R M / M A G
 ******************************************************************************/

#define LIB_LINALG_VEC_GETNORM(vec, field, len, out) \
    do { \
        *(out) = 0; \
        for (uint8_t _i = 0U; _i < (len); _i++) { \
            *(out) += (vec)->field[_i] * (vec)->field[_i]; \
        } \
        *(out) = sqrtf(*(out)); \
    } while (0)

#define LIB_LINALG_GETNORM_CVEC(vec, out) \
    LIB_LINALG_VEC_GETNORM(vec, elemCol, COLS(vec), out)

#define LIB_LINALG_GETNORM_RVEC(vec, out) \
    LIB_LINALG_VEC_GETNORM(vec, elemRow, ROWS(vec), out)

/******************************************************************************
 *                          M A T R I X  O P S
 ******************************************************************************/

#define LIB_LINALG_CHECK_SQUARE_RMAT(mat) \
    (MROWS(mat) == MCOLS(mat))

#define LIB_LINALG_CHECK_SIZE_RMAT(mat, n) \
    (MROWS(mat) == (n) && MCOLS(mat) == (n))

#define LIB_LINALG_SETIDENTITY_RMAT(mat) \
    _Static_assert(LIB_LINALG_CHECK_SQUARE_RMAT(mat), "Matrix must be square"); \
    do { \
        for (uint8_t _r = 0U; _r < MROWS(mat); _r++) { \
            for (uint8_t _c = 0U; _c < MCOLS(mat); _c++) { \
                (mat)->rows[_r][_c] = (_r == _c) ? 1.0f : 0.0f; \
            } \
        } \
    } while (0)

#define LIB_LINALG_MUL_RMATCVEC(rmat, col, out) \
    _Static_assert(MCOLS(rmat) == COLS(col), "Matrix/vector size mismatch"); \
    _Static_assert(COLS(out) == MROWS(rmat), "Output vector size mismatch"); \
    do { \
        for (uint8_t _r = 0U; _r < MROWS(rmat); _r++) { \
            for (uint8_t _c = 0U; _c < MCOLS(rmat); _c++) { \
                (out)->elemCol[_r] += (rmat)->rows[_r][_c] * (col)->elemCol[_c]; \
            } \
        } \
    } while (0)

#define LIB_LINALG_MUL_RMATCVEC_SET(rmat, col, out) \
    do { \
        LIB_LINALG_CLEAR_CVEC(out); \
        LIB_LINALG_MUL_RMATCVEC(rmat, col, out); \
    } while (0)

#define LIB_LINALG_MUL_RMATRMAT_SET(rmatA, rmatB, out) \
    _Static_assert(MCOLS(rmatA) == MROWS(rmatB), "Matrix/matrix size mismatch"); \
    _Static_assert(MROWS(out) == MROWS(rmatA), "Output matrix row size mismatch"); \
    _Static_assert(MCOLS(out) == MCOLS(rmatB), "Output matrix col size mismatch"); \
    do { \
        for (uint8_t _r = 0U; _r < MROWS(out); _r++) { \
            for (uint8_t _c = 0U; _c < MCOLS(out); _c++) { \
                (out)->rows[_r][_c] = 0; \
            } \
        } \
        for (uint8_t _r = 0U; _r < MROWS(rmatA); _r++) { \
            for (uint8_t _c = 0U; _c < MCOLS(rmatB); _c++) { \
                for (uint8_t _k = 0U; _k < MCOLS(rmatA); _k++) { \
                    (out)->rows[_r][_c] += \
                        (rmatA)->rows[_r][_k] * (rmatB)->rows[_k][_c]; \
                } \
            } \
        } \
    } while (0)
