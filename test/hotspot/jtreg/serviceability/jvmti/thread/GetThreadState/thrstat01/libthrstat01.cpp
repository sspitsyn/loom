/*
 * Copyright (c) 2004, 2022, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <stdio.h>
#include <string.h>
#include "jvmti.h"
#include "jvmti_common.h"

extern "C" {

#define WAIT_START 100
#define WAIT_TIME (2*60*1000)

static jvmtiEnv *jvmti = NULL;

static jrawMonitorID access_lock;
static jrawMonitorID wait_lock;
static jthread tested_thread_thr1 = NULL;

static jint state[] = {
    JVMTI_THREAD_STATE_RUNNABLE,
    JVMTI_THREAD_STATE_BLOCKED_ON_MONITOR_ENTER,
    JVMTI_THREAD_STATE_IN_OBJECT_WAIT
};

void JNICALL VMInit(jvmtiEnv *jvmti_env, JNIEnv *jni, jthread thr) {
  jvmtiError err = jvmti_env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, NULL);
  check_jvmti_status(jni, err, "Error in SetEventNotificationMode");
}

void JNICALL
ThreadStart(jvmtiEnv *jvmti_env, JNIEnv *jni, jthread thread) {

  RawMonitorLocker rml = RawMonitorLocker(jvmti_env, jni, access_lock);
  jvmtiThreadInfo thread_info = get_thread_info(jvmti, jni, thread);

  if (thread_info.name != NULL && strcmp(thread_info.name, "tested_thread_thr1") == 0) {
    tested_thread_thr1 = jni->NewGlobalRef(thread);
    LOG(">>> ThreadStart: \"%s\", 0x%p\n", thread_info.name, tested_thread_thr1);
    jvmtiError err = jvmti_env->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_THREAD_START, NULL);
    check_jvmti_status(jni, err, "Error in SetEventNotificationMode");
  }
}

jint Agent_OnLoad(JavaVM *jvm, char *options, void *reserved) {
  jint res;
  jvmtiError err;

  LOG("Agent_OnLoad started\n");

  res = jvm->GetEnv((void **) &jvmti, JVMTI_VERSION_1_1);
  if (res != JNI_OK || jvmti == NULL) {
    LOG("Wrong result of a valid call to GetEnv!\n");
    return JNI_ERR;
  }

  access_lock = create_raw_monitor(jvmti, "_access_lock");
  wait_lock = create_raw_monitor(jvmti, "_wait_lock");

  jvmtiEventCallbacks callbacks;
  callbacks.VMInit = &VMInit;
  callbacks.ThreadStart = &ThreadStart;
  err = jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
  if (err != JVMTI_ERROR_NONE) {
    LOG("(SetEventCallbacks) unexpected error: %s (%d)\n", TranslateError(err), err);
    return JNI_ERR;
  }

  err = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
  if (err != JVMTI_ERROR_NONE) {
    LOG("(SetEventNotificationMode) unexpected error: %s (%d)\n", TranslateError(err), err);
    return JNI_ERR;
  }
  LOG("Agent_OnLoad finished\n");
  return JNI_OK;
}

JNIEXPORT jboolean JNICALL
Java_thrstat01_checkStatus0(JNIEnv *jni, jclass cls, jint stat_ind) {
  jboolean result = JNI_TRUE;
  jint thread_state;
  jint millis;

  LOG("native method checkStatus started\n");

  if (tested_thread_thr1 == NULL) {
    LOG("Missing thread \"tested_thread_thr1\" start event\n");
    return JNI_FALSE;
  }

  /* wait until thread gets an expected state */
  for (millis = WAIT_START; millis < WAIT_TIME; millis <<= 1) {
    thread_state = get_thread_state(jvmti, jni, tested_thread_thr1);
    if ((thread_state & state[stat_ind]) != 0) {
      break;
    }
    RawMonitorLocker rml = RawMonitorLocker(jvmti, jni, wait_lock);
    rml.wait(millis);
  }

  LOG(">>> thread \"tested_thread_thr1\" (0x%p) state: %s (%d)\n", tested_thread_thr1, TranslateState(thread_state), thread_state);

  if ((thread_state & state[stat_ind]) == 0) {
    LOG("Wrong thread \"tested_thread_thr1\" (0x%p) state:\n", tested_thread_thr1);
    LOG("    expected: %s (%d)\n", TranslateState(state[stat_ind]), state[stat_ind]);
    LOG("      actual: %s (%d)\n", TranslateState(thread_state), thread_state);
    result = JNI_FALSE;
  }
  LOG("native method checkStatus finished\n");
  return result;
}

}
