/*
 * Copyright (C) 2026 The Android Open Source Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy at http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 */

// Darwin AOT uses the switch interpreter. Keep distinct entrypoint addresses
// for method classification, and trap any accidental fast-interpreter entry.
.text
.p2align 4
.globl _ExecuteNterpImpl, _EndExecuteNterpImpl
.private_extern _ExecuteNterpImpl, _EndExecuteNterpImpl
_ExecuteNterpImpl:
    ud2
_EndExecuteNterpImpl:
.p2align 4
.globl _ExecuteNterpWithClinitImpl, _EndExecuteNterpWithClinitImpl
.private_extern _ExecuteNterpWithClinitImpl, _EndExecuteNterpWithClinitImpl
_ExecuteNterpWithClinitImpl:
    ud2
_EndExecuteNterpWithClinitImpl:
.globl _artNterpAsmInstructionStart, _artNterpAsmInstructionEnd
.private_extern _artNterpAsmInstructionStart, _artNterpAsmInstructionEnd
_artNterpAsmInstructionStart:
_artNterpAsmInstructionEnd:
