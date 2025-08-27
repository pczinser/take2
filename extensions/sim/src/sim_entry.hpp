#pragma once
#include <cstdint>
#include <dmsdk/dlib/buffer.h>
#include <dmsdk/dlib/message.h>
#include "command_queue.hpp"  // For Command type

namespace simcore {

// Listener registration
void SetListenerURL(const dmMessage::URL& url);

// Snapshots
dmBuffer::HBuffer GetSnapshotBuffer();          // current (legacy)
dmBuffer::HBuffer GetSnapshotBufferCurrent();   // current
dmBuffer::HBuffer GetSnapshotBufferPrevious();  // previous
uint32_t          GetSnapshotRowCount();

// Timing
uint32_t GetCurrentTick();
float    GetFixedDt();
float    GetLastAlpha();                        // interpolation leftover [0..1)

// Optional debug controls
void SetSimHz(double hz);
void SetSimPaused(bool paused);
void StepSimNTicks(int n);

// Command queue
void EnqueueCommand(const Command& cmd);

} // namespace simcore
