// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "internal/platform/implementation/windows/log_message.h"

#include <algorithm>

namespace nearby {
    namespace windows {

        api::LogMessage::Severity min_log_severity_ = api::LogMessage::Severity::kInfo;

        //// Variables of type LogSeverity are widely taken to lie in the range
        //// [0, NUM_SEVERITIES-1].  Be careful to preserve this assumption if
        //// you ever need to change their values or add a new severity.
        //typedef int LogSeverity;
        //
        //const int GLOG_INFO = 0, GLOG_WARNING = 1, GLOG_ERROR = 2, GLOG_FATAL = 3,
        //NUM_SEVERITIES = 4;

        int ConvertToGLogSeverity(LogMessage::Severity severity)
        {
            int result{ 0 };
            switch (severity)
            {
            case LogMessage::Severity::kInfo:
                return 0;
            case LogMessage::Severity::kWarning:
                return 1;
            case LogMessage::Severity::kError:
                return 2;
            case LogMessage::Severity::kFatal:
                return 3;
            }
            return result;
        }

        LogMessage::LogMessage(const char* file, int line, Severity severity)
            : log_streamer_(file, line, ConvertToGLogSeverity(severity)) {}

        LogMessage::~LogMessage() = default;

        void LogMessage::Print(const char* format, ...) {
        }

        std::ostream& LogMessage::Stream() { return log_streamer_.stream(); }

    }  // namespace windows

    namespace api {

        void LogMessage::SetMinLogSeverity(Severity severity) {
            windows::min_log_severity_ = severity;
        }

        bool LogMessage::ShouldCreateLogMessage(Severity severity) {
            return severity >= windows::min_log_severity_;
        }
    }  // namespace api
}  // namespace nearby