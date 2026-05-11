#include "lib_linAlg.h"
#include "unity.h"

LIB_LINALG_DEFINE_N(vec2, float, 2U);
LIB_LINALG_DEFINE_N(vec3, float, 3U);
LIB_LINALG_DEFINE_NM(mat23, float, 2U, 3U);

void setUp(void)
{
}

void tearDown(void)
{
}

static void vector_helpers_sum_scale_dot_and_norm(void)
{
    linAlg_vec3_cvec_S a = { .elemCol = { 1.0f, 2.0f, 3.0f } };
    linAlg_vec3_cvec_S b = { .elemCol = { 3.0f, 4.0f, 5.0f } };
    linAlg_vec3_cvec_S out = { 0 };
    linAlg_vec3_rvec_S row = { .elemRow = { 2.0f, 3.0f, 4.0f } };
    float dot = 10.0f;
    float norm = 0.0f;

    LIB_LINALG_SUM_CVEC(&a, &b, &out);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, out.elemCol[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, out.elemCol[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.0f, out.elemCol[2]);

    LIB_LINALG_MUL_CVECSCALAR(&a, 2.0f, &out);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, out.elemCol[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, out.elemCol[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, out.elemCol[2]);

    LIB_LINALG_MUL_RVECCVEC_SET(&row, &a, &dot);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, dot);

    LIB_LINALG_GETNORM_CVEC(&a, &norm);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.741657f, norm);
    LIB_LINALG_CLEAR_CVEC(&out);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out.elemCol[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out.elemCol[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, out.elemCol[2]);
}

static void matrix_helpers_set_identity_and_multiply_vectors(void)
{
    linAlg_vec2_rmat_S identity = { 0 };
    linAlg_mat23_rmat_S mat = {
        .rows = {
            { 1.0f, 2.0f, 3.0f },
            { 4.0f, 5.0f, 6.0f },
        },
    };
    linAlg_vec3_cvec_S in = { .elemCol = { 1.0f, 2.0f, 3.0f } };
    linAlg_mat23_cvec_S out = { 0 };

    LIB_LINALG_SETIDENTITY_RMAT(&identity);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, identity.rows[0][0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, identity.rows[0][1]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, identity.rows[1][0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, identity.rows[1][1]);

    LIB_LINALG_MUL_RMATCVEC_SET(&mat, &in, &out);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.0f, out.elemCol[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 32.0f, out.elemCol[1]);
}

static void weighted_average_and_copy_operate_elementwise(void)
{
    linAlg_vec2_cvec_S a = { .elemCol = { 10.0f, 20.0f } };
    linAlg_vec2_cvec_S b = { .elemCol = { 2.0f, 4.0f } };
    linAlg_vec2_cvec_S out = { 0 };

    LIB_LINALG_WEIGHTAVG_CVEC(&a, &b, 0.25f, &out);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, out.elemCol[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 8.0f, out.elemCol[1]);

    LIB_LINALG_CVEC_EQ_CVEC(&a, &out);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, out.elemCol[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 20.0f, out.elemCol[1]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(vector_helpers_sum_scale_dot_and_norm);
    RUN_TEST(matrix_helpers_set_identity_and_multiply_vectors);
    RUN_TEST(weighted_average_and_copy_operate_elementwise);
    return UNITY_END();
}

