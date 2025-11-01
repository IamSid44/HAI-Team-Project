#ifndef VPU_H
#define VPU_H

#include "systemc.h"
#include <cmath>     // For std::exp, std::sqrt, std::max
#include <vector>
#include <limits>    // For std::numeric_limits (used for -infinity)

// Template parameter 'VECTOR_SIZE' makes the VPU re-usable
template <unsigned int VECTOR_SIZE>
SC_MODULE(VectorProcessingUnit) {

    // --- Ports ---
    sc_in<bool> clk;         // Clock
    sc_in<bool> rst_n;       // Asynchronous active-low reset
    sc_in<bool> start;       // Start signal to begin processing
    sc_out<bool> done;       // Signal to indicate processing is complete

    // Operation select: 0 = Normalization, 1 = Softmax-Weighted-Sum
    sc_in<sc_uint<1>> op_sel; 

    // --- Vector I/O Ports ---
    // Inputs for scores (used by both ops)
    sc_vector<sc_in<double>> vec_in_scores;
    // Inputs for values (used only by softmax-sum)
    sc_vector<sc_in<double>> vec_in_values; 

    // --- Output Ports ---
    // Vector output for Normalization
    sc_vector<sc_out<double>> vec_out_norm; 
    // Scalar output for Softmax-Weighted-Sum
    sc_out<double> scalar_out_softmax;

    // --- Internal Process ---
    void processing_thread() {
        // --- Reset ---
        done.write(false);
        scalar_out_softmax.write(0.0);
        for (unsigned int i = 0; i < VECTOR_SIZE; i++) {
            vec_out_norm[i].write(0.0);
        }
        wait(); // Wait for reset to de-assert

        // --- Main processing loop ---
        while (true) {
            // --- IDLE State ---
            while (start.read() == false) {
                wait();
            }

            // --- PROCESSING State ---
            done.write(false);
            wait(); // Spend 1 cycle latching 'start'

            // --- Operation Selection ---
            if (op_sel.read() == 0) {
                // --- Operation 0: L2 Normalization ---
                // This logic is unchanged.
                // It reads from vec_in_scores and writes to vec_out_norm.

                // Pass 1: Read inputs AND calculate Sum-of-Squares
                double sum_sq = 0.0;
                double internal_vec[VECTOR_SIZE];
                for (unsigned int i = 0; i < VECTOR_SIZE; i++) {
                    double val = vec_in_scores[i].read();
                    internal_vec[i] = val; // Store for pass 2
                    sum_sq += val * val;
                    wait(); // Model 1 cycle per read/MAC
                }

                // Calculate the L2 norm (sqrt)
                double norm = std::sqrt(sum_sq);
                wait(); // Model 1 cycle for the sqrt operation

                // Pass 2: Divide each element by the norm
                for (unsigned int i = 0; i < VECTOR_SIZE; i++) {
                    double result = (norm == 0.0) ? 0.0 : (internal_vec[i] / norm);
                    vec_out_norm[i].write(result);
                    wait(); // Model 1 cycle per division/write
                }
                
            } else {
                // --- Operation 1: Softmax Weighted Sum (FlashAttention-style) ---
                // This is the new single-pass logic based on your image.
                // $o'_i = o'_{i-1} \frac{d'_{i-1}}{d'_i} e^{m_{i-1}-m_i} + \frac{e^{x_i-m_i}}{d'_i} v_i$
                // We simplify this to:
                // $o_i = (o_{i-1} \cdot d_{i-1} \cdot e^{m_{i-1}-m_i}) + (e^{x_i-m_i} \cdot v_i)$
                // ...and then divide the *final* $o_N$ by the *final* $d_N$
                // A safer way is to use the exact equation.

                // Initialization of state variables
                double m_i = -std::numeric_limits<double>::infinity(); // Current max
                double d_i = 0.0; // Current sum of exponentials (denominator)
                double o_i = 0.0; // Current accumulated output (numerator)

                double m_prev, d_prev, o_prev;

                // --- Single Pass Loop ---
                // This loop reads scores and values, and computes the 
                // weighted sum on-the-fly without storing the softmax vector.
                for (unsigned int i = 0; i < VECTOR_SIZE; i++) {
                    // Store previous state
                    m_prev = m_i;
                    d_prev = d_i;
                    o_prev = o_i;
                    
                    // Read current score (x_i) and value (v_i)
                    double x_i = vec_in_scores[i].read();
                    double v_i = vec_in_values[i].read();

                    // --- Update m_i and d_i ---
                    // $m_i = \max(m_{i-1}, x_i)$
                    m_i = std::max(m_prev, x_i); 

                    // $d'_i = d'_{i-1} e^{m_{i-1}-m_i} + e^{x_i-m_i}$
                    double exp_mprev_mi = std::exp(m_prev - m_i);
                    double exp_xi_mi = std::exp(x_i - m_i);
                    
                    d_i = (d_prev * exp_mprev_mi) + exp_xi_mi;

                    // --- Update o_i ---
                    // $o'_i = o'_{i-1} \frac{d'_{i-1}}{d'_i} e^{m_{i-1}-m_i} + \frac{e^{x_i-m_i}}{d'_i} v_i$
                    // We can rewrite this to avoid division until the end:
                    // Let $o_i = o'_i \cdot d'_i$ (the un-normalized output)
                    // $o_i = (o'_{i-1} \cdot d'_{i-1}) e^{m_{i-1}-m_i} + e^{x_i-m_i} v_i$
                    // $o_i = o_{i-1} e^{m_{i-1}-m_i} + e^{x_i-m_i} v_i$
                    o_i = (o_prev * exp_mprev_mi) + (exp_xi_mi * v_i);
                    
                    wait(); // Model 1 cycle per element
                }
                
                // --- Final Output Calculation ---
                // The loop computed the final un-normalized output $o_N$
                // and the final denominator $d_N$. Now we do the one-and-only division.
                // Final $o'_N = o_N / d_N$
                double final_output = (d_i == 0.0) ? 0.0 : (o_i / d_i);

                // Write the final scalar result
                scalar_out_softmax.write(final_output);
            }

            // --- DONE State ---
            done.write(true);
            wait(); // Hold 'done' high for one cycle

            while (start.read() == true) {
                wait();
            }
            done.write(false);
        }
    }

    // --- Constructor ---
    SC_CTOR(VectorProcessingUnit) {
        // Initialize all vector ports
        vec_in_scores.init(VECTOR_SIZE);
        vec_in_values.init(VECTOR_SIZE);
        vec_out_norm.init(VECTOR_SIZE);

        // Register the processing thread
        SC_CTHREAD(processing_thread, clk.pos());
        reset_signal_is(rst_n, false); // Active-low reset
    }
};

#endif // VPU_H