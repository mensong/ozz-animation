//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) Guillaume Blanc                                              //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#ifndef OZZ_OZZ_ANIMATION_RUNTIME_SAMPLING_JOB_H_
#define OZZ_OZZ_ANIMATION_RUNTIME_SAMPLING_JOB_H_

#include "ozz/animation/runtime/export.h"
#include "ozz/base/platform.h"
#include "ozz/base/span.h"

namespace ozz {

// Forward declaration of math structures.
namespace math {
struct SoaTransform;
}

namespace animation {

// Forward declares the animation type to sample.
class Animation;

// Samples an animation at a given time ratio in the unit interval [0,1] (where
// 0 is the beginning of the animation, 1 is the end), to output the
// corresponding posture in local-space.
// SamplingJob uses a context (aka SamplingJob::Context) to store intermediate
// values (decompressed animation keyframes...) while sampling. This context
// also stores pre-computed values that allows drastic optimization while
// playing/sampling the animation forward. Backward sampling works, but isn't
// optimized through the context. The job does not owned the buffers (in/output)
// and will thus not delete them during job's destruction.
struct OZZ_ANIMATION_DLL SamplingJob {
  // Validates job parameters. Returns true for a valid job, or false otherwise:
  // -if any input pointer is nullptr
  // -if output range is invalid.
  bool Validate() const;

  // Runs job's sampling task.
  // The job is validated before any operation is performed, see Validate() for
  // more details.
  // Returns false if *this job is not valid.
  bool Run() const;

  // Time ratio in the unit interval [0,1] used to sample animation (where 0 is
  // the beginning of the animation, 1 is the end). It should be computed as the
  // current time in the animation , divided by animation duration.
  // This ratio is clamped before job execution in order to resolves any
  // approximation issue on range bounds.
  float ratio = 0.f;

  // The animation to sample.
  const Animation* animation = nullptr;

  // Forward declares the context object used by the SamplingJob.
  class Context;

  // A context object that must be big enough to sample *this animation.
  Context* context = nullptr;

  // Job output.
  // The output range to be filled with sampled joints during job execution.
  // If there are less joints in the animation compared to the output range,
  // then remaining SoaTransform are left unchanged.
  // If there are more joints in the animation, then the last joints are not
  // sampled.
  span<ozz::math::SoaTransform> output;
};

namespace internal {
// Soa hot data to interpolate.
struct InterpSoaFloat3;
struct InterpSoaQuaternion;
}  // namespace internal

// Declares the context object used by the workload to take advantage of the
// frame coherency of animation sampling.
class OZZ_ANIMATION_DLL SamplingJob::Context {
 public:
  // Constructs an empty context. The context needs to be resized with the
  // appropriate number of tracks before it can be used with a SamplingJob.
  Context();

  // Constructs a context that can be used to sample any animation with at most
  // _max_tracks tracks. _num_tracks is internally aligned to a multiple of
  // soa size, which means max_tracks() can return a different (but bigger)
  // value than _max_tracks.
  explicit Context(int _max_tracks);

  // Disables copy and assignation.
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(Context&&) = delete;

  // Deallocates context.
  ~Context();

  // Resize the number of joints that the context can support.
  // This also implicitly invalidate the context.
  void Resize(int _max_tracks);

  // Invalidate the context.
  // The SamplingJob automatically invalidates a context when required
  // during sampling. This automatic mechanism is based on the animation
  // address and sampling time ratio. The weak point is that it can result in a
  // crash if ever the address of an animation is used again with another
  // animation (could be the result of successive call to delete / new).
  // Therefore it is recommended to manually invalidate a context when it is
  // known that this context will not be used for with an animation again.
  void Invalidate();

  // The maximum number of tracks that the context can handle.
  int max_tracks() const { return max_soa_tracks_ * 4; }
  int max_soa_tracks() const { return max_soa_tracks_; }

  struct Cache {
    // Points to the keys in the animation that are valid for the current time
    // ratio.
    span<uint32_t> entries;

    // Outdated soa entries. One bit per soa entry (32 joints per byte).
    span<byte> outdated;

    // Next key to process in the animation.
    uint32_t next;
  };

 private:
  friend struct SamplingJob;

  // Steps the context in order to use it for a potentially new animation. If
  // the _animation is different from the animation currently cached, then the
  // context is invalidated and reset for the new _animation and _ratio.
  // Return previous ratio.
  float Step(const Animation& _animation, float _ratio);

  void Deallocate();

  // The animation this context refers to. nullptr means that the context is
  // invalid.
  const Animation* animation_;

  // The current time ratio in the animation.
  float ratio_;

  // The number of soa tracks that can store this context.
  int max_soa_tracks_;

  // Single allocation for the whole context.
  void* allocation_ = nullptr;

  // Context cache instances per component.
  Cache translations_cache_;
  Cache rotations_cache_;
  Cache scales_cache_;

  // SoA hot decompressed data to interpolate.
  span<internal::InterpSoaFloat3> translations_;
  span<internal::InterpSoaQuaternion> rotations_;
  span<internal::InterpSoaFloat3> scales_;
};
}  // namespace animation
}  // namespace ozz
#endif  // OZZ_OZZ_ANIMATION_RUNTIME_SAMPLING_JOB_H_
