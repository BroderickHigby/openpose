#ifndef OPENPOSE_WRAPPER_WRAPPER_STRUCT_TRACKING_HPP
#define OPENPOSE_WRAPPER_WRAPPER_STRUCT_TRACKING_HPP

#include <openpose/core/common.hpp>

namespace op
{
    /**
     * Add what it does in here (follow examples from the others)
     */
    struct OP_API WrapperStructTracking
    {
        /**
         * Add what it does in here.
         */
        int tracking;

        /**
         * Constructor of the struct.
         * It has the recommended and default values we recommend for each element of the struct.
         * Since all the elements of the struct are public, they can also be manually filled.
         */
        WrapperStructTracking(
            const int tracking = -1);
    };
}

#endif // OPENPOSE_WRAPPER_WRAPPER_STRUCT_TRACKING_HPP
