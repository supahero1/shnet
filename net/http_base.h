/*
   Copyright 2021 shädam

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef iEC_LncxD5VOn_Pzbzr2OainjEm_c760
#define iEC_LncxD5VOn_Pzbzr2OainjEm_c760 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct HTTP_settings {
  uint32_t max_path_length;          // 4096
  uint32_t max_header_amount;        // 64
  uint32_t max_header_name_length;   // 64
  uint32_t max_header_value_length;  // 4096
  uint32_t max_reason_phrase_length; // 64
  uint32_t max_body_length;          // 256 * 4096 (1.05MB)
};

#ifdef __cplusplus
}
#endif

#endif // iEC_LncxD5VOn_Pzbzr2OainjEm_c760