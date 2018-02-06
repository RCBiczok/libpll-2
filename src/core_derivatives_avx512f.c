/*
    Copyright (C) 2016 Tomas Flouri, Diego Darriba, Alexey Kozlov

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Contact: Tomas Flouri <Tomas.Flouri@h-its.org>,
    Exelixis Lab, Heidelberg Instutute for Theoretical Studies
    Schloss-Wolfsbrunnenweg 35, D-69118 Heidelberg, Germany
*/
#include <limits.h>
#include "pll.h"

void print_256d(const __m256d v) {
  const double* val = (const double*)&v;
  printf("% .3e % .3e % .3e % .3e\n",
         val[3], val[2], val[1], val[0]);
}

void print_256i(const __m256i v) {
  const uint32_t *val = (const uint32_t *) &v;
  printf("%u %u %u %u %u %u %u %u\n",
         val[7], val[6], val[5], val[4], val[3], val[2], val[1], val[0]);
}

void print_512d(const __m512d v) {
  const double *val = (const double *) &v;
  printf("% .3e % .3e % .3e % .3e % .3e % .3e % .3e % .3e\n",
         val[7], val[6], val[5], val[4], val[3], val[2], val[1], val[0]);
}

void print_512i(const __m512i v) {
  const uint64_t *val = (const uint64_t *) &v;
  printf("%lu %lu %lu %lu %lu %lu %lu %lu\n",
         val[7], val[6], val[5], val[4], val[3], val[2], val[1], val[0]);
}

void print_512d_half(const __m512d v, size_t half) {
  const double *val = (const double *) &v;
  printf("% .3e % .3e % .3e % .3e\n",
         val[3 + 4 * half], val[2 + 4 * half], val[1 + 4 * half], val[0 + 4 * half]);
}

inline double reduce_add_pd(const __m512d zmm) {
  __m256d low = _mm512_castpd512_pd256(zmm);
  __m256d high = _mm512_extractf64x4_pd(zmm, 1);

  __m256d a = _mm256_add_pd(low, high);
  __m256d t1 = _mm256_hadd_pd(a, a);
  __m128d t2 = _mm256_extractf128_pd(t1, 1);
  __m128d t3 = _mm_add_sd(_mm256_castpd256_pd128(t1), t2);
  return _mm_cvtsd_f64(t3);
}

#define COMPUTE_GATHER_MASK(n, sites, elems_per_reg) \
  ((n) + (elems_per_reg) <= (sites) ? 0xff : 0xff >> ((elems_per_reg) - (sites) % (elems_per_reg)))

#define COMPUTE_II_QCOL(q, offset) \
/* row 0 */ \
v_mat    = _mm512_load_pd(lm0 + (offset)); \
v_lterm0 = _mm512_fmadd_pd(v_mat, v_lclv[q], v_lterm0); \
v_mat    = _mm512_load_pd(rm0 + (offset)); \
v_rterm0 = _mm512_fmadd_pd(v_mat, v_rclv[q], v_rterm0); \
\
/* row 1 */ \
v_mat    = _mm512_load_pd(lm1 + (offset)); \
v_lterm1 = _mm512_fmadd_pd(v_mat, v_lclv[q], v_lterm1); \
v_mat    = _mm512_load_pd(rm1 + (offset)); \
v_rterm1 = _mm512_fmadd_pd(v_mat, v_rclv[q], v_rterm1); \
\
/* row 2 */ \
v_mat    = _mm512_load_pd(lm2 + (offset)); \
v_lterm2 = _mm512_fmadd_pd(v_mat, v_lclv[q], v_lterm2); \
v_mat    = _mm512_load_pd(rm2 + (offset)); \
v_rterm2 = _mm512_fmadd_pd(v_mat, v_rclv[q], v_rterm2); \
\
/* row 3 */ \
v_mat    = _mm512_load_pd(lm3 + (offset)); \
v_lterm3 = _mm512_fmadd_pd(v_mat, v_lclv[q], v_lterm3); \
v_mat    = _mm512_load_pd(rm3 + (offset)); \
v_rterm3 = _mm512_fmadd_pd(v_mat, v_rclv[q], v_rterm3); \
\
/* row 4 */ \
v_mat    = _mm512_load_pd(lm4 + (offset)); \
v_lterm4 = _mm512_fmadd_pd(v_mat, v_lclv[q], v_lterm4); \
v_mat    = _mm512_load_pd(rm4 + (offset)); \
v_rterm4 = _mm512_fmadd_pd(v_mat, v_rclv[q], v_rterm4); \
\
/* row 5 */ \
v_mat    = _mm512_load_pd(lm5 + (offset)); \
v_lterm5 = _mm512_fmadd_pd(v_mat, v_lclv[q], v_lterm5); \
v_mat    = _mm512_load_pd(rm5 + (offset)); \
v_rterm5 = _mm512_fmadd_pd(v_mat, v_rclv[q], v_rterm5); \
\
/* row 6 */ \
v_mat    = _mm512_load_pd(lm6 + (offset)); \
v_lterm6 = _mm512_fmadd_pd(v_mat, v_lclv[q], v_lterm6); \
v_mat    = _mm512_load_pd(rm6 + (offset)); \
v_rterm6 = _mm512_fmadd_pd(v_mat, v_rclv[q], v_rterm6); \
\
/* row 7 */ \
v_mat    = _mm512_load_pd(lm7 + (offset)); \
v_lterm7 = _mm512_fmadd_pd(v_mat, v_lclv[q], v_lterm7); \
v_mat    = _mm512_load_pd(rm7 + (offset)); \
v_rterm7 = _mm512_fmadd_pd(v_mat, v_rclv[q], v_rterm7);

#define COMPUTE_STATE_PART(i, j, k) \
v_inv_eigenvecs = _mm512_set1_pd(tt_inv_eigenvecs[(i) * states * states \
                                                          + (j) * states \
                                                          + (k)]); \
v_eigenvecs = _mm512_set1_pd(tt_eigenvecs[(i) * states * states \
                                                  + (j) * states \
                                                  + (k)]); \
v_lefterm = _mm512_fmadd_pd(v_clvp[k], v_inv_eigenvecs, v_lefterm); \
v_righterm = _mm512_fmadd_pd(v_clvc[k], v_eigenvecs, v_righterm); \

PLL_EXPORT int pll_core_update_sumtable_ii_20x20_avx512f(unsigned int sites,
                                                         unsigned int rate_cats,
                                                         const double *clvp,
                                                         const double *clvc,
                                                         const unsigned int *parent_scaler,
                                                         const unsigned int *child_scaler,
                                                         double *const *eigenvecs,
                                                         double *const *inv_eigenvecs,
                                                         double *const *freqs,
                                                         double *sumtable,
                                                         unsigned int attrib) {
  const double *t_clvp = clvp;
  const double *t_clvc = clvc;

  double *sum = sumtable;

  unsigned int states = 20;
  unsigned int states_padded = (states + 7) & (0xFFFFFFFF - 7);

  /* scaling stuff */
  __m512i min_scaler =  _mm512_setzero_epi32();
  __m512i *rate_scalings = NULL;
  int per_rate_scaling = (attrib & PLL_ATTRIB_RATE_SCALERS) ? 1 : 0;

  /* powers of scale threshold for undoing the scaling */
  double scale_minlh[PLL_SCALE_RATE_MAXDIFF];
  if (per_rate_scaling) {
    rate_scalings = (__m512i *) pll_aligned_alloc(rate_cats * sizeof(__m512i), PLL_ALIGNMENT_AVX512F);
    if (!rate_scalings) {
      pll_errno = PLL_ERROR_MEM_ALLOC;
      snprintf(pll_errmsg, 200, "Cannot allocate memory for rate_scalings");
      return PLL_FAILURE;
    }
    //rate_scalings = (__m512i *) calloc(rate_cats, sizeof(__m512i));

    double scale_factor = 1.0;
    for (unsigned int i = 0; i < PLL_SCALE_RATE_MAXDIFF; ++i) {
      scale_factor *= PLL_SCALE_THRESHOLD;
      scale_minlh[i] = scale_factor;
    }
  }

  /* padded eigenvecs */
  double *tt_eigenvecs = (double *) pll_aligned_alloc(
          (states * states * rate_cats) * sizeof(double),
          PLL_ALIGNMENT_AVX512F);

  if (!tt_eigenvecs) {
    pll_errno = PLL_ERROR_MEM_ALLOC;
    snprintf(pll_errmsg, 200, "Cannot allocate memory for tt_eigenvecs");
    return PLL_FAILURE;
  }

  /* transposed padded inv_eigenvecs */
  double *tt_inv_eigenvecs = (double *) pll_aligned_alloc(
          (states * states * rate_cats) * sizeof(double),
          PLL_ALIGNMENT_AVX512F);

  if (!tt_inv_eigenvecs) {
    pll_errno = PLL_ERROR_MEM_ALLOC;
    snprintf(pll_errmsg, 200, "Cannot allocate memory for tt_inv_eigenvecs");
    return PLL_FAILURE;
  }

  /* broadcast eigenvecs matrices and multiply with frequencies */
  for (unsigned int i = 0; i < rate_cats; ++i) {
    for (unsigned int j = 0; j < states; ++j)
      for (unsigned int k = 0; k < states; ++k) {
        tt_inv_eigenvecs[i * states * states +
                         j * states +
                         k] = inv_eigenvecs[i][k * states_padded + j] * freqs[i][k];
        tt_eigenvecs[i * states * states +
                     j * states +
                     k] = eigenvecs[i][j * states_padded + k];
      }
  }

  __m512i v_index = _mm512_setr_epi64(0,
                                      1 * rate_cats * states_padded,
                                      2 * rate_cats * states_padded,
                                      3 * rate_cats * states_padded,
                                      4 * rate_cats * states_padded,
                                      5 * rate_cats * states_padded,
                                      6 * rate_cats * states_padded,
                                      7 * rate_cats * states_padded);

  __m512i v_scaler_index = _mm512_setr_epi64(0,
                                             1 * rate_cats,
                                             2 * rate_cats,
                                             3 * rate_cats,
                                             4 * rate_cats,
                                             5 * rate_cats,
                                             6 * rate_cats,
                                             7 * rate_cats);

  /* build sumtable */
  for (unsigned int n = 0; n < sites; n += ELEM_PER_AVX515_REGISTER) {
    __mmask8 gather_mask = COMPUTE_GATHER_MASK(n, sites, ELEM_PER_AVX515_REGISTER);
    if (per_rate_scaling) {
      /* compute minimum per-rate scaler -> common per-site scaler */
      min_scaler =  _mm512_set1_epi64(UINT_MAX);
      //min_scaler = UINT_MAX;
      for (unsigned int i = 0; i < rate_cats; ++i) {
        rate_scalings[i] = _mm512_setzero_epi32();

        if(parent_scaler) {
          __m512i v_parent_scaler =  _mm512_cvtepi32_epi64(_mm512_mask_i64gather_epi32(_mm256_setzero_si256(),
                                                           gather_mask,
                                                           v_scaler_index,
                                                           (void*)(parent_scaler + n * rate_cats + i),
                                                           sizeof(unsigned int)));
          rate_scalings[i] = v_parent_scaler;
        }

        //rate_scalings[i] = (parent_scaler) ? parent_scaler[n*rate_cats+i] : 0;
        if(child_scaler) {
          __m512i v_child_scaler =  _mm512_cvtepi32_epi64(_mm512_mask_i64gather_epi32(_mm256_setzero_si256(),
                                                          gather_mask,
                                                          v_scaler_index,
                                                          (void*)(child_scaler + n * rate_cats + i),
                                                          sizeof(unsigned int)));

          rate_scalings[i] = _mm512_add_epi64(rate_scalings[i], v_child_scaler);
        }
        //rate_scalings[i] += (child_scaler) ? child_scaler[n*rate_cats+i] : 0;

        min_scaler = _mm512_mask_blend_epi64(_mm512_cmplt_epi64_mask(rate_scalings[i], min_scaler),
                                             min_scaler,
                                             rate_scalings[i]);
        //if (rate_scalings[i] < min_scaler)
        //  min_scaler = rate_scalings[i];
      }

      /* compute relative capped per-rate scalers */
      for (unsigned int i = 0; i < rate_cats; ++i) {
        rate_scalings[i] = _mm512_min_epi64(_mm512_sub_epi32(rate_scalings[i], min_scaler),
                                            _mm512_set1_epi64(PLL_SCALE_RATE_MAXDIFF));
        //rate_scalings[i] = PLL_MIN(rate_scalings[i] - min_scaler, 
        //                           PLL_SCALE_RATE_MAXDIFF);
      }
    }

    for (unsigned int i = 0; i < rate_cats; ++i) {
      __m512d v_clvp[states];
      __m512d v_clvc[states];

      v_clvp[0] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp, sizeof(double));
      v_clvc[0] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc, sizeof(double));
      v_clvp[1] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 1, sizeof(double));
      v_clvc[1] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 1, sizeof(double));
      v_clvp[2] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 2, sizeof(double));
      v_clvc[2] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 2, sizeof(double));
      v_clvp[3] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 3, sizeof(double));
      v_clvc[3] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 3, sizeof(double));
      v_clvp[4] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 4, sizeof(double));
      v_clvc[4] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 4, sizeof(double));
      v_clvp[5] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 5, sizeof(double));
      v_clvc[5] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 5, sizeof(double));
      v_clvp[6] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 6, sizeof(double));
      v_clvc[6] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 6, sizeof(double));
      v_clvp[7] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 7, sizeof(double));
      v_clvc[7] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 7, sizeof(double));
      v_clvp[8] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 8, sizeof(double));
      v_clvc[8] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 8, sizeof(double));
      v_clvp[9] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 9, sizeof(double));
      v_clvc[9] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 9, sizeof(double));
      v_clvp[10] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 10, sizeof(double));
      v_clvc[10] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 10, sizeof(double));
      v_clvp[11] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 11, sizeof(double));
      v_clvc[11] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 11, sizeof(double));
      v_clvp[12] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 12, sizeof(double));
      v_clvc[12] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 12, sizeof(double));
      v_clvp[13] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 13, sizeof(double));
      v_clvc[13] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 13, sizeof(double));
      v_clvp[14] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 14, sizeof(double));
      v_clvc[14] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 14, sizeof(double));
      v_clvp[15] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 15, sizeof(double));
      v_clvc[15] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 15, sizeof(double));
      v_clvp[16] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 16, sizeof(double));
      v_clvc[16] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 16, sizeof(double));
      v_clvc[17] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 17, sizeof(double));
      v_clvp[17] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 17, sizeof(double));
      v_clvp[18] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 18, sizeof(double));
      v_clvc[18] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 18, sizeof(double));
      v_clvp[19] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + 19, sizeof(double));
      v_clvc[19] = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + 19, sizeof(double));

      for (unsigned int j = 0; j < states; ++j) {
        __m512d v_lefterm = _mm512_setzero_pd();
        __m512d v_righterm = _mm512_setzero_pd();

        __m512d v_inv_eigenvecs;
        __m512d v_eigenvecs;

        COMPUTE_STATE_PART(i, j, 0);
        COMPUTE_STATE_PART(i, j, 1);
        COMPUTE_STATE_PART(i, j, 2);
        COMPUTE_STATE_PART(i, j, 3);
        COMPUTE_STATE_PART(i, j, 4);
        COMPUTE_STATE_PART(i, j, 5);
        COMPUTE_STATE_PART(i, j, 6);
        COMPUTE_STATE_PART(i, j, 7);
        COMPUTE_STATE_PART(i, j, 8);
        COMPUTE_STATE_PART(i, j, 9);
        COMPUTE_STATE_PART(i, j, 10);
        COMPUTE_STATE_PART(i, j, 11);
        COMPUTE_STATE_PART(i, j, 12);
        COMPUTE_STATE_PART(i, j, 13);
        COMPUTE_STATE_PART(i, j, 14);
        COMPUTE_STATE_PART(i, j, 15);
        COMPUTE_STATE_PART(i, j, 16);
        COMPUTE_STATE_PART(i, j, 17);
        COMPUTE_STATE_PART(i, j, 18);
        COMPUTE_STATE_PART(i, j, 19);

        __m512d v_sum = _mm512_mul_pd(v_lefterm, v_righterm);

        if (rate_scalings) {
          __mmask8 scaling_mask = _mm512_cmpgt_epi64_mask(rate_scalings[i], _mm512_setzero_si512());
          __m512d v_prod = _mm512_fmadd_pd(v_sum,
                                           _mm512_mask_i64gather_pd(_mm512_setzero_pd(),
                                                                    scaling_mask,
                                                                    _mm512_sub_epi64(rate_scalings[i], _mm512_set1_epi64(1)),
                                                                    scale_minlh,
                                                                    sizeof(double)),
                                           _mm512_setzero_pd());

          v_sum = _mm512_mask_blend_pd(scaling_mask, v_sum, v_prod);
        }
        //if (rate_scalings && rate_scalings[i] > 0)
        //  sum[j] *= scale_minlh[rate_scalings[i]-1];

        _mm512_store_pd(sum, v_sum);
        sum += ELEM_PER_AVX515_REGISTER;
      }
      t_clvc += states_padded;
      t_clvp += states_padded;
    }
    //pointers already moved one site ahead, move another 7 sites forward,
    //so we start at the date of the 9th state
    t_clvc += rate_cats * states_padded * (ELEM_PER_AVX515_REGISTER - 1);
    t_clvp += rate_cats * states_padded * (ELEM_PER_AVX515_REGISTER - 1);
  }

  if (rate_scalings)
    free(rate_scalings);

  return PLL_SUCCESS;
}

PLL_EXPORT int pll_core_update_sumtable_ii_avx512f(unsigned int states,
                                                   unsigned int sites,
                                                   unsigned int rate_cats,
                                                   const double *clvp,
                                                   const double *clvc,
                                                   const unsigned int *parent_scaler,
                                                   const unsigned int *child_scaler,
                                                   double *const *eigenvecs,
                                                   double *const *inv_eigenvecs,
                                                   double *const *freqs,
                                                   double *sumtable,
                                                   unsigned int attrib) {
  const double *t_clvp = clvp;
  const double *t_clvc = clvc;

  /* dedicated functions for 4x4 and 20x20 matrices */
  if (states == 4) {
    /* call AVX variant */
    return pll_core_update_sumtable_ii_avx(states,
                                           sites,
                                           rate_cats,
                                           clvp,
                                           clvc,
                                           parent_scaler,
                                           child_scaler,
                                           eigenvecs,
                                           inv_eigenvecs,
                                           freqs,
                                           sumtable,
                                           attrib);
  } else if (states == 20) {
    return pll_core_update_sumtable_ii_20x20_avx512f(sites,
                                                     rate_cats,
                                                     clvp,
                                                     clvc,
                                                     parent_scaler,
                                                     child_scaler,
                                                     eigenvecs,
                                                     inv_eigenvecs,
                                                     freqs,
                                                     sumtable,
                                                     attrib);
  }

  double *sum = sumtable;

  unsigned int states_padded = (states + 7) & (0xFFFFFFFF - 7);

  /* scaling stuff */
  unsigned int min_scaler = 0;
  unsigned int *rate_scalings = NULL;
  int per_rate_scaling = (attrib & PLL_ATTRIB_RATE_SCALERS) ? 1 : 0;

  /* powers of scale threshold for undoing the scaling */
  double scale_minlh[PLL_SCALE_RATE_MAXDIFF];
  if (per_rate_scaling) {
    rate_scalings = (unsigned int *) calloc(rate_cats, sizeof(unsigned int));

    double scale_factor = 1.0;
    for (unsigned int i = 0; i < PLL_SCALE_RATE_MAXDIFF; ++i) {
      scale_factor *= PLL_SCALE_THRESHOLD;
      scale_minlh[i] = scale_factor;
    }
  }

  /* padded eigenvecs */
  double *tt_eigenvecs = (double *) pll_aligned_alloc(
      (states * states * rate_cats) * sizeof(double),
      PLL_ALIGNMENT_AVX512F);

  if (!tt_eigenvecs) {
    pll_errno = PLL_ERROR_MEM_ALLOC;
    snprintf(pll_errmsg, 200, "Cannot allocate memory for tt_eigenvecs");
    return PLL_FAILURE;
  }

  /* transposed padded inv_eigenvecs */
  double *tt_inv_eigenvecs = (double *) pll_aligned_alloc(
      (states * states * rate_cats) * sizeof(double),
      PLL_ALIGNMENT_AVX512F);

  if (!tt_inv_eigenvecs) {
    pll_errno = PLL_ERROR_MEM_ALLOC;
    snprintf(pll_errmsg, 200, "Cannot allocate memory for tt_inv_eigenvecs");
    return PLL_FAILURE;
  }

  /* broadcast eigenvecs matrices and multiply with frequencies */
  for (unsigned int i = 0; i < rate_cats; ++i) {
    for (unsigned int j = 0; j < states; ++j)
      for (unsigned int k = 0; k < states; ++k) {
        tt_inv_eigenvecs[i * states * states + j * states + k] =
            inv_eigenvecs[i][k * states_padded + j] * freqs[i][k];
        tt_eigenvecs[i * states * states + j * states + k] = eigenvecs[i][j
            * states_padded + k];
      }
  }

  //TODO
  if (per_rate_scaling) {
    printf("Per rate scaling not supported in AVX512\n");
    exit(1);
  }

  __m512i v_index = _mm512_setr_epi64(0, 1 * rate_cats * states_padded,
      2 * rate_cats * states_padded, 3 * rate_cats * states_padded,
      4 * rate_cats * states_padded, 5 * rate_cats * states_padded,
      6 * rate_cats * states_padded, 7 * rate_cats * states_padded);

  /* build sumtable */
  for (unsigned int n = 0; n < sites; n += ELEM_PER_AVX515_REGISTER) {
    __mmask8 gather_mask = COMPUTE_GATHER_MASK(n, sites, ELEM_PER_AVX515_REGISTER);
    for (unsigned int i = 0; i < rate_cats; ++i) {
      for (unsigned int j = 0; j < states; ++j) {
        __m512d v_lefterm = _mm512_setzero_pd();
        __m512d v_righterm = _mm512_setzero_pd();

        for (unsigned int k = 0; k < states; ++k) {
          __m512d v_clvp = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvp + k, sizeof(double));
          __m512d v_clvc = _mm512_mask_i64gather_pd(_mm512_setzero_pd(), gather_mask, v_index, t_clvc + k, sizeof(double));

          __m512d v_inv_eigenvecs = _mm512_set1_pd(tt_inv_eigenvecs[i * states * states + j * states + k]);
          __m512d v_eigenvecs = _mm512_set1_pd(tt_eigenvecs[i * states * states + j * states + k]);
          v_lefterm = _mm512_fmadd_pd(v_clvp, v_inv_eigenvecs, v_lefterm);
          v_righterm = _mm512_fmadd_pd(v_clvc, v_eigenvecs, v_righterm);
        }

        __m512d v_sum = _mm512_mul_pd(v_lefterm, v_righterm);

        _mm512_store_pd(sum, v_sum);
        sum += ELEM_PER_AVX515_REGISTER;
      }
      t_clvc += states_padded;
      t_clvp += states_padded;
    }
    //pointers already moved one site ahead, move another 7 sites forward,
    //so we start at the date of the 9th state
    t_clvc += rate_cats * states_padded * (ELEM_PER_AVX515_REGISTER - 1);
    t_clvp += rate_cats * states_padded * (ELEM_PER_AVX515_REGISTER - 1);
  }

  if (rate_scalings)
    free(rate_scalings);

  return PLL_SUCCESS;
}

PLL_EXPORT int pll_core_update_sumtable_ti_avx512f(unsigned int states,
                                                   unsigned int sites,
                                                   unsigned int rate_cats,
                                                   const double *parent_clv,
                                                   const unsigned char *left_tipchars,
                                                   const unsigned int *parent_scaler,
                                                   double *const *eigenvecs,
                                                   double *const *inv_eigenvecs,
                                                   double *const *freqs,
                                                   const pll_state_t *tipmap,
                                                   unsigned int tipmap_size,
                                                   double *sumtable,
                                                   unsigned int attrib) {
  //TODO: Not implemented!
  assert(!(attrib & PLL_ATTRIB_PATTERN_TIP));
  return PLL_FAILURE;
}

PLL_EXPORT int pll_core_update_sumtable_repeats_generic_avx512f(unsigned int states,
                                                                unsigned int sites,
                                                                unsigned int parent_sites,
                                                                unsigned int rate_cats,
                                                                const double * clvp,
                                                                const double * clvc,
                                                                const unsigned int * parent_scaler,
                                                                const unsigned int * child_scaler,
                                                                double * const * eigenvecs,
                                                                double * const * inv_eigenvecs,
                                                                double * const * freqs,
                                                                double *sumtable,
                                                                const unsigned int * parent_site_id,
                                                                const unsigned int * child_site_id,
                                                                double * bclv_buffer,
                                                                unsigned int inv,
                                                                unsigned int attrib)
{
  //TODO: Not implemented!
  assert(!(attrib & PLL_ATTRIB_SITE_REPEATS));
  return PLL_FAILURE;
}

PLL_EXPORT
int pll_core_likelihood_derivatives_avx512f(unsigned int states,
                                            unsigned int states_padded,
                                            unsigned int rate_cats,
                                            unsigned int ef_sites,
                                            const unsigned int *pattern_weights,
                                            const double *rate_weights,
                                            const int *invariant,
                                            const double *prop_invar,
                                            double *const *freqs,
                                            const double *sumtable,
                                            const double *diagptable,
                                            double *d_f,
                                            double *dd_f) {
  /* vectors for accumulating LH, 1st and 2nd derivatives */
  __m512d v_df = _mm512_setzero_pd();
  __m512d v_ddf = _mm512_setzero_pd();
  __m512d v_all1 = _mm512_set1_pd(1.);

  __m512d site_lk[3];

  const double *sum = sumtable;
  const int *invariant_ptr = invariant;

  double *t_diagp = (double *) pll_aligned_alloc(
          ELEM_PER_AVX515_REGISTER * 3 * rate_cats * states * sizeof(double), PLL_ALIGNMENT_AVX512F);

  if (!t_diagp) {
    pll_errno = PLL_ERROR_MEM_ALLOC;
    snprintf(pll_errmsg, 200, "Unable to allocate enough memory.");
    return PLL_FAILURE;
  }

  /* transpose diagptable */
  for (unsigned int i = 0; i < rate_cats; ++i) {
    for (unsigned int j = 0; j < states; ++j) {
      for (unsigned int k = 0; k < 3; ++k) {
        __m512d v_diagp = _mm512_set1_pd(diagptable[i * states * 4 + j * 4 + k]);
        _mm512_store_pd(t_diagp +
                        i * ELEM_PER_AVX515_REGISTER * 3 * states +
                        j * ELEM_PER_AVX515_REGISTER * 3 +
                        k * ELEM_PER_AVX515_REGISTER,
                        v_diagp);
      }
    }
  }

  for (unsigned int n = 0;
       n < ef_sites;
       n += ELEM_PER_AVX515_REGISTER, invariant_ptr += ELEM_PER_AVX515_REGISTER) {
    site_lk[0] = _mm512_setzero_pd();
    site_lk[1] = _mm512_setzero_pd();
    site_lk[2] = _mm512_setzero_pd();

    const double *diagp = t_diagp;

    for (unsigned int i = 0; i < rate_cats; ++i) {

      __m512d v_cat_sitelk[3];
      v_cat_sitelk[0] = _mm512_setzero_pd();
      v_cat_sitelk[1] = _mm512_setzero_pd();
      v_cat_sitelk[2] = _mm512_setzero_pd();

      for (unsigned int j = 0;
           j < states; j++, diagp += 3 * ELEM_PER_AVX515_REGISTER, sum += ELEM_PER_AVX515_REGISTER) {
        __m512d v_sum = _mm512_load_pd(sum);
        __m512d v_diagp;

        v_diagp = _mm512_load_pd(diagp);
        v_cat_sitelk[0] = _mm512_fmadd_pd(v_sum, v_diagp, v_cat_sitelk[0]);

        v_diagp = _mm512_load_pd(diagp + ELEM_PER_AVX515_REGISTER);
        v_cat_sitelk[1] = _mm512_fmadd_pd(v_sum, v_diagp, v_cat_sitelk[1]);

        v_diagp = _mm512_load_pd(diagp + 2 * ELEM_PER_AVX515_REGISTER);
        v_cat_sitelk[2] = _mm512_fmadd_pd(v_sum, v_diagp, v_cat_sitelk[2]);
      }

      /* account for invariant sites */
      double t_prop_invar = prop_invar[i];
      if (t_prop_invar > 0) {

        //TODO Vectorize?
        double inv_site_lk_0 =
                (n + 0 >= ef_sites || invariant_ptr[0] == -1) ? 0 : freqs[i][invariant_ptr[0]] * t_prop_invar;
        double inv_site_lk_1 =
                (n + 1 >= ef_sites || invariant_ptr[1] == -1) ? 0 : freqs[i][invariant_ptr[1]] * t_prop_invar;
        double inv_site_lk_2 =
                (n + 2 >= ef_sites || invariant_ptr[2] == -1) ? 0 : freqs[i][invariant_ptr[2]] * t_prop_invar;
        double inv_site_lk_3 =
                (n + 3 >= ef_sites || invariant_ptr[3] == -1) ? 0 : freqs[i][invariant_ptr[3]] * t_prop_invar;
        double inv_site_lk_4 =
                (n + 4 >= ef_sites || invariant_ptr[4] == -1) ? 0 : freqs[i][invariant_ptr[4]] * t_prop_invar;
        double inv_site_lk_5 =
                (n + 5 >= ef_sites || invariant_ptr[5] == -1) ? 0 : freqs[i][invariant_ptr[5]] * t_prop_invar;
        double inv_site_lk_6 =
                (n + 6 >= ef_sites || invariant_ptr[6] == -1) ? 0 : freqs[i][invariant_ptr[6]] * t_prop_invar;
        double inv_site_lk_7 =
                (n + 7 >= ef_sites || invariant_ptr[7] == -1) ? 0 : freqs[i][invariant_ptr[7]] * t_prop_invar;

        __m512d v_inv_site_lk = _mm512_setr_pd(inv_site_lk_0,
                                               inv_site_lk_1,
                                               inv_site_lk_2,
                                               inv_site_lk_3,
                                               inv_site_lk_4,
                                               inv_site_lk_5,
                                               inv_site_lk_6,
                                               inv_site_lk_7);

        __m512d v_prop_invar = _mm512_set1_pd(1. - t_prop_invar);

        v_cat_sitelk[0] = _mm512_add_pd(_mm512_mul_pd(v_cat_sitelk[0], v_prop_invar), v_inv_site_lk);
        v_cat_sitelk[1] = _mm512_mul_pd(v_cat_sitelk[1], v_prop_invar);
        v_cat_sitelk[2] = _mm512_mul_pd(v_cat_sitelk[2], v_prop_invar);
      }

      /* apply rate category weights */
      __m512d v_weight = _mm512_set1_pd(rate_weights[i]);
      site_lk[0] = _mm512_fmadd_pd(v_cat_sitelk[0], v_weight, site_lk[0]);
      site_lk[1] = _mm512_fmadd_pd(v_cat_sitelk[1], v_weight, site_lk[1]);
      site_lk[2] = _mm512_fmadd_pd(v_cat_sitelk[2], v_weight, site_lk[2]);
    }

    /* build derivatives */
    __m512d v_recip0 = _mm512_div_pd(v_all1, site_lk[0]);
    __m512d v_deriv1 = _mm512_mul_pd(site_lk[1], v_recip0);
    __m512d v_deriv2 = _mm512_sub_pd(_mm512_mul_pd(v_deriv1, v_deriv1),
                                     _mm512_mul_pd(site_lk[2], v_recip0));

    /* eliminates nan values on padded states */
    if (n + ELEM_PER_AVX515_REGISTER > ef_sites) {
      __mmask8 mask = _mm512_cmp_pd_mask(site_lk[0], _mm512_setzero_pd(), _CMP_NEQ_UQ);

      v_deriv1 = _mm512_maskz_expand_pd(mask, v_deriv1);
      v_deriv2 = _mm512_maskz_expand_pd(mask, v_deriv2);
    }

    v_df = _mm512_fnmadd_pd(v_deriv1, _mm512_set1_pd(pattern_weights[n]), v_df);
    v_ddf = _mm512_fmadd_pd(v_deriv2, _mm512_set1_pd(pattern_weights[n]), v_ddf);
  }

  *d_f = reduce_add_pd(v_df);
  *dd_f = reduce_add_pd(v_ddf);

  return PLL_SUCCESS;
}