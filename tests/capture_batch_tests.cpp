#include "capture_batch.hpp"
#include <iostream>
#include <stdexcept>
using namespace cl;
static int checks = 0;
static void check(bool ok, const char *what) {
    if (!ok)
        throw std::runtime_error(what);
    ++checks;
}
int main() {
    try {
        const std::array<QByteArray, 3> frames{QByteArray("\0one", 4), "second", "third"};
        int nativeCalls = 0, singleCalls = 0;
        auto native = [&](BatchSendResult result) {
            ++nativeCalls;
            return result;
        };
        check(dispatchLocalBatch(
                  {}, [&] { return native(BatchSendResult::Sent); },
                  [&](const auto &) {
                      ++singleCalls;
                      return true;
                  }),
              "empty batch succeeds");
        check(nativeCalls == 0 && singleCalls == 0, "empty batch does not touch driver");
        check(dispatchLocalBatch(
                  frames, [&] { return native(BatchSendResult::Sent); },
                  [&](const auto &) {
                      ++singleCalls;
                      return true;
                  }),
              "native batch succeeds");
        check(nativeCalls == 1 && singleCalls == 0, "native success never repeats individual packets");
        QList<QByteArray> received;
        check(dispatchLocalBatch(
                  frames, [&] { return native(BatchSendResult::Unavailable); },
                  [&](const auto &b) {
                      received.append(b);
                      return true;
                  }),
              "unavailable API falls back");
        check(received == QList<QByteArray>{frames[0], frames[1], frames[2]},
              "fallback preserves binary bytes and order");
        check(!dispatchLocalBatch(
                  frames, [&] { return native(BatchSendResult::Failed); },
                  [&](const auto &) {
                      ++singleCalls;
                      return true;
                  }),
              "native failure is returned");
        check(singleCalls == 0, "partial or failed native send never resends whole batch");
        received.clear();
        check(!dispatchLocalBatch(
                  frames, [] { return BatchSendResult::Unavailable; },
                  [&](const auto &b) {
                      received.append(b);
                      return received.size() < 2;
                  }),
              "fallback send failure is returned");
        check(received == QList<QByteArray>{frames[0], frames[1]}, "fallback stops on first error");
        check(dispatchLocalBatch(
                  frames, [] { return BatchSendResult::Sent; },
                  [](const auto &) {
                      throw std::runtime_error("unexpected duplicate");
                      return false;
                  }),
              "next batch remains independent");
        std::cout << "PASS " << checks << " local batch checks\n";
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
