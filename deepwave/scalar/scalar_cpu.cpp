#include <stddef.h>
#include <string.h>
#include "scalar.h"
#include "scalar_device.h"

/* Dimension-specific definitions
 *
 * LOOP: Loops over shots and spatial dimensions
 * ENDLOOP: Close the loops in LOOP
 * location_index: Convert array of coordinates into index into flat array
 * */
#if DIM == 1

#define LOOP(num_shots, start_z, end_z, start_y, end_y, start_x, end_x) \
        for (ptrdiff_t shot = 0; shot < num_shots; shot++) \
{ \
        for (ptrdiff_t z = start_z; z < end_z; z++) \
        { \
                ptrdiff_t y = 0; \
                ptrdiff_t x = 0;

#define ENDLOOP \
        } \
}

inline ptrdiff_t location_index(
                const ptrdiff_t *__restrict__ const arr,
                const ptrdiff_t *__restrict__ const shape,
                const ptrdiff_t index)
{
        const ptrdiff_t z = arr[index];

        return z;
}

#elif DIM == 2

#define LOOP(num_shots, start_z, end_z, start_y, end_y, start_x, end_x) \
        for (ptrdiff_t shot = 0; shot < num_shots; shot++) \
{ \
        for (ptrdiff_t z = start_z; z < end_z; z++) \
        { \
                for (ptrdiff_t y = start_y; y < end_y; y++) \
                { \
                        ptrdiff_t x = 0;


#define ENDLOOP \
                } \
        } \
}

inline ptrdiff_t location_index(
                const ptrdiff_t *__restrict__ const arr,
                const ptrdiff_t *__restrict__ const shape,
                const ptrdiff_t index)
{
        const ptrdiff_t z = arr[index * 2];
        const ptrdiff_t y = arr[index * 2 + 1];

        return z * shape[1] + y;
}

#elif DIM == 3

#define LOOP(num_shots, start_z, end_z, start_y, end_y, start_x, end_x) \
        for (ptrdiff_t shot = 0; shot < num_shots; shot++) \
{ \
        for (ptrdiff_t z = start_z; z < end_z; z++) \
        { \
                for (ptrdiff_t y = start_y; y < end_y; y++) \
                { \
                        for (ptrdiff_t x = start_x; x < end_x; x++) \
                        { \

#define ENDLOOP \
                        } \
                } \
        } \
}

inline ptrdiff_t location_index(
                const ptrdiff_t *__restrict__ const arr,
                const ptrdiff_t *__restrict__ const shape,
                const ptrdiff_t index)
{
        const ptrdiff_t z = arr[index * 3];
        const ptrdiff_t y = arr[index * 3 + 1];
        const ptrdiff_t x = arr[index * 3 + 2];

        return z * shape[1] * shape[2] + y * shape[2] + x;
}

#else
#error "Must specify the dimension, e.g. -D DIM=1"
#endif /* DIM */

void setup(
		const float *__restrict__ const fd1,
		const float *__restrict__ const fd2)
{
}

void propagate(
                float *__restrict__ const wfn, /* next wavefield */
                float *__restrict__ const auxn, /* next auxiliary */
                const float *__restrict__ const wfc, /* current wavefield */
                const float *__restrict__ const wfp, /* previous wavefield */
                const float *__restrict__ const auxc, /* current auxiliary */
                const float *__restrict__ const sigma,
                const float *__restrict__ const model,
                const float *__restrict__ const fd1, /* 1st difference coeffs */
                const float *__restrict__ const fd2, /* 2nd difference coeffs */
                const ptrdiff_t *__restrict__ const shape,
                const ptrdiff_t *__restrict__ const pml_width,
                const ptrdiff_t num_shots,
                const float dt)
{

        const ptrdiff_t numel_shot = shape[0] * shape[1] * shape[2];
        const ptrdiff_t size_x = shape[2];
        const ptrdiff_t size_xy = shape[1] * shape[2];
        float *__restrict__ const phizn = auxn;
        const float *__restrict__ const phizc = auxc;
        const float *__restrict__ const sigmaz = sigma;

#if DIM >= 2

        float *__restrict__ const phiyn = auxn + num_shots * numel_shot;
        const float *__restrict__ const phiyc = auxc + num_shots * numel_shot;
        const float *__restrict__ const sigmay = sigma + shape[0];

#endif /* DIM >= 2 */

#if DIM == 3

        float *__restrict__ const phixn = auxn + 2 * num_shots * numel_shot;
        float *__restrict__ const psin = auxn + 3 * num_shots * numel_shot;
        const float *__restrict__ const phixc = auxc + 2 * num_shots * numel_shot;
        const float *__restrict__ const psic = auxc + 3 * num_shots * numel_shot;
        const float *__restrict__ const sigmax = sigma + shape[0] + shape[1];

#endif /* DIM  == 3 */

#pragma omp parallel for default(none) collapse(2)

        LOOP(num_shots,
                        ZPAD, shape[0] - ZPAD,
                        YPAD, shape[1] - YPAD,
                        XPAD, shape[2] - XPAD);

        ptrdiff_t i = z * size_xy + y * size_x + x;
        ptrdiff_t si = shot * numel_shot + i;

        /* Spatial finite differences */
        float lap = laplacian(wfc);
        float wfc_z = z_deriv(wfc);
        float phizc_z = z_deriv(phizc);

#if DIM == 1

        /* Update wavefield */
        wfn[si] = 1 / (1 + dt * sigmaz[z] / 2) *
                (model[i] * (lap + phizc_z)
                 + dt * sigmaz[z] * wfp[si] / 2
                 + (2 * wfc[si] - wfp[si]));

        /* Update phi */
        phizn[si] = phizc[si] - dt * sigmaz[z] * (wfc_z + phizc[si]);

#elif DIM == 2

        float wfc_y = y_deriv(wfc);
        float phiyc_y = y_deriv(phiyc);

        /* Update wavefield */
        wfn[si] = 1 / (1 + dt * (sigmaz[z] + sigmay[y]) / 2) *
                (model[i] * (lap + phizc_z + phiyc_y) +
                 dt * (sigmaz[z] + sigmay[y]) * wfp[si] / 2 +
                 (2 * wfc[si] - wfp[si]) -
                 dt * dt * sigmaz[z] * sigmay[y] * wfc[si]);

        /* Update phi */
        phizn[si] = phizc[si] - dt * (sigmaz[z] * phizc[si] +
                        (sigmaz[z] - sigmay[y]) * wfc_z);
        phiyn[si] = phiyc[si] - dt * (sigmay[y] * phiyc[si] +
                        (sigmay[y] - sigmaz[z]) * wfc_y);

#elif DIM == 3

        float wfc_y = y_deriv(wfc);
        float wfc_x = x_deriv(wfc);
        float phiyc_y = y_deriv(phiyc);
        float phixc_x = x_deriv(phixc);
        float psic_z = z_deriv(psic);
        float psic_y = y_deriv(psic);
        float psic_x = x_deriv(psic);

        /* Update wavefield */
        wfn[si] = 1 / (1 + dt * (sigmaz[z] + sigmay[y] + sigmax[x]) / 2) *
                (model[i] * lap + dt * dt * (phizc_z + phiyc_y + phixc_x -
                                             sigmaz[z] * sigmay[y] * sigmax[x] *
                                             psic[si]) +
                 dt * (sigmaz[z] + sigmay[y] + sigmax[x]) * wfp[si] / 2 +
                 (2 * wfc[si] - wfp[si]) -
                 dt * dt * wfc[si] *
                 (sigmax[x] * sigmay[y] +
                  sigmay[y] * sigmaz[z] +
                  sigmax[x] * sigmaz[z]));

        /* Update phi */
        phizn[si] = phizc[si] - dt * sigmaz[z] * phizc[si] +
                model[i] / dt * (sigmay[y] + sigmax[x]) * wfc_z +
                dt * sigmax[x] * sigmay[y] * psic_z;
        phiyn[si] = phiyc[si] - dt * sigmay[y] * phiyc[si] +
                model[i] / dt * (sigmaz[z] + sigmax[x]) * wfc_y +
                dt * sigmax[x] * sigmaz[z] * psic_y;
        phixn[si] = phixc[si] - dt * sigmax[x] * phixc[si] +
                model[i] / dt * (sigmaz[z] + sigmay[y]) * wfc_x +
                dt * sigmaz[z] * sigmay[y] * psic_x;

        /* Update psi */
        psin[si] = psic[si] + dt * wfc[si];

#endif /* DIM */

        ENDLOOP;

}


void add_sources(
                float *__restrict__ const next_wavefield,
                const float *__restrict__ const model,
                const float *__restrict__ const source_amplitudes,
                const ptrdiff_t *__restrict__ const source_locations,
                const ptrdiff_t *__restrict__ const shape,
                const ptrdiff_t num_shots,
                const ptrdiff_t num_sources_per_shot)
{

        for (ptrdiff_t shot = 0; shot < num_shots; shot++)
        {
                for (ptrdiff_t source = 0; source < num_sources_per_shot;
                                source++)
                {

                        ptrdiff_t s = shot * num_sources_per_shot + source;
                        ptrdiff_t i = location_index(source_locations, shape,
                                        s);
                        ptrdiff_t si = shot * shape[0] * shape[1] * shape[2] +
                                i;
                        next_wavefield[si] += source_amplitudes[s] * model[i];

                }
        }

}


void record_receivers(
                float *__restrict__ const receiver_amplitudes,
                const float *__restrict__ const current_wavefield,
                const ptrdiff_t *__restrict__ const receiver_locations,
                const ptrdiff_t *__restrict__ const shape,
                const ptrdiff_t num_shots,
                const ptrdiff_t num_receivers_per_shot)
{

        if (receiver_amplitudes == NULL) return; /* no source inversion */

        for (ptrdiff_t shot = 0; shot < num_shots; shot++)
        {
                for (ptrdiff_t receiver = 0; receiver < num_receivers_per_shot;
                                receiver++)
                {

                        ptrdiff_t r = shot * num_receivers_per_shot + receiver;
                        ptrdiff_t si = shot * shape[0] * shape[1] * shape[2] +
                                location_index(receiver_locations, shape, r);
                        receiver_amplitudes[r] = current_wavefield[si];

                }
        }

}


void save_wavefields(
                float *__restrict__ const saved_wavefields,
                const float *__restrict__ const current_wavefield,
                const ptrdiff_t *__restrict__ const shape,
                const ptrdiff_t num_shots,
                const ptrdiff_t step,
                const enum wavefield_save_strategy save_strategy)
{

        if (save_strategy == STRATEGY_COPY)
        {
                float *__restrict__ current_saved_wavefield = set_step_pointer(
                                saved_wavefields,
                                step,
                                num_shots,
                                shape[0] * shape[1] * shape[2]);
                memcpy(current_saved_wavefield, current_wavefield,
                                num_shots * shape[0] * shape[1] * shape[2] *
                                sizeof(float));
        }

}


void imaging_condition(
                float *__restrict__ const model_grad,
                const float *__restrict__ const current_wavefield,
                const float *__restrict__ const next_adjoint_wavefield,
                const float *__restrict__ const current_adjoint_wavefield,
                const float *__restrict__ const previous_adjoint_wavefield,
                const ptrdiff_t *__restrict__ const shape,
                const ptrdiff_t *__restrict__ const pml_width,
                const ptrdiff_t num_shots,
                const float dt)
{

        if (model_grad == NULL) return; /* Not doing model inversion */

        const ptrdiff_t grad_size_z = shape[0] - 2 * ZPAD -
                pml_width[0] - pml_width[1];
        const ptrdiff_t grad_size_y = shape[1] - 2 * YPAD -
                pml_width[2] - pml_width[3];
        const ptrdiff_t grad_size_x = shape[2] - 2 * XPAD -
                pml_width[4] - pml_width[5];
        const ptrdiff_t grad_size_xy = grad_size_y * grad_size_x;

        LOOP(num_shots, 0, grad_size_z, 0, grad_size_y, 0, grad_size_x);

        ptrdiff_t si = shot * shape[0] * shape[1] * shape[2] +
                (z + ZPAD + pml_width[0]) * shape[1] * shape[2] +
                (y + YPAD + pml_width[2]) * shape[2] +
                x + XPAD + pml_width[4];

        float adjoint_wavefield_tt = (next_adjoint_wavefield[si] -
                        2 * current_adjoint_wavefield[si] +
                        previous_adjoint_wavefield[si]) / (dt * dt);

        model_grad[z * grad_size_xy + y * grad_size_x + x] +=
                current_wavefield[si] *	adjoint_wavefield_tt;

        ENDLOOP;

}


void model_grad_scaling(
                float *__restrict__ const model_grad,
                const float *__restrict__ const scaling,
                const ptrdiff_t *__restrict__ const shape,
                const ptrdiff_t *__restrict__ const pml_width)
{

        if (model_grad == NULL) return; /* Not doing model inversion */

        const ptrdiff_t grad_size_z = shape[0] - 2 * ZPAD -
                pml_width[0] - pml_width[1];
        const ptrdiff_t grad_size_y = shape[1] - 2 * YPAD -
                pml_width[2] - pml_width[3];
        const ptrdiff_t grad_size_x = shape[2] - 2 * XPAD -
                pml_width[4] - pml_width[5];
        ptrdiff_t grad_size = grad_size_z * grad_size_y * grad_size_x;

        for (ptrdiff_t i = 0; i < grad_size; i++)
        {
                model_grad[i] *= scaling[i];
        }

}
