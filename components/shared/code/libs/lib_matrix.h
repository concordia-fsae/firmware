/**
 * @file lib_matrix.h
 * @brief Header file for matrix library
 */

#pragma once

/******************************************************************************
 *                             I N C L U D E S
 ******************************************************************************/

#include "lib_utility.h"

/******************************************************************************
 *                              D E F I N E S
 ******************************************************************************/

#define LIB_MATRIX_DEFINE(name, type, elements) \
    typedef struct { \
        type vector[elements]; \
    } name##_vector_S; \
    typedef struct { \
        name##_vector_S rows[elements]; \
    } name##_matrix_S 

#define LIB_MATRIX_INST(name) \
    name##_matrix_S name

#define LIB_VECTOR_INST(name) \
    name##_vector_S name

#define LIB_VECTOR_ADD(first, second, output) \
    for (int i=0; i<COUNTOF((output)->vector); i++){ \
        (output)->vector[i] = (first)->vector[i]+(second)->vector[i]; \
    }

#define LIB_VECTOR_SUBTRACT(first, second, output) \
    for (int i=0; i<COUNTOF((output)->vector); i++){ \
        (output)->vector[i] = (first)->vector[i]-(second)->vector[i]; \
    }

#define LIB_MATRIX_ADD(first, second, output) \
    for (int j=0; j<COUNTOF((output)->rows); j++){ \
        LIB_VECTOR_ADD( &((first)->rows[j]), &((second)->rows[j]), &((output)->rows[j])); \
    }
    
#define LIB_MATRIX_SUBTRACT(first, second, output) \
    for (int i=0; i<COUNTOF((output)->rows); i++){ \
        for (int j=0; j<COUNTOF((output)->rows[i].vector); j++){ \
            (output)->rows[i].vector[j] = (first)->rows[i].vector[j]-(second)->rows[i].vector[j]; \
        } \
    }
    
#define LIB_VECTOR_PEEK(name, pos) (name)->vector[pos];

#define LIB_MATRIX_PEEK(name, row, col) (name)->rows[row].vector[col];
    
