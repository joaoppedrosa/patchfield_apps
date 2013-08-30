/*
 * Copyright 2013 Google Inc. All Rights Reserved.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy
 * of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#include "javamodule.h"

#include "audio_module.h"
#include "utils/buffer_size_adapter.h"

#include <semaphore.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  buffer_size_adapter *bsa;
  sem_t wake;
  sem_t ready;
  const float *input_buffer;
  float *output_buffer;
  int done;
} jmodule;

static void process_jm(void *context, int sample_rate, int buffer_frames,
    int input_channels, const float *input_buffer,
    int output_channels, float *output_buffer) {
  jmodule *jm = (jmodule *) context;
  jm->input_buffer = input_buffer;
  jm->output_buffer = output_buffer;
  sem_post(&jm->wake);
  sem_wait(&jm->ready);
}

JNIEXPORT jlong JNICALL
Java_com_noisepages_nettoyeur_patchfield_java_JavaModule_configure
(JNIEnv *env, jobject obj, jlong p, jint host_buffer_size,
 jint user_buffer_size, jint input_channels, jint output_channels) {
  jmodule *jm = malloc(sizeof(jmodule));
  if (jm) {
    sem_init(&jm->wake, 0, 0);
    sem_init(&jm->ready, 0, 0);
    jm->input_buffer = NULL;
    jm->output_buffer = NULL;
    jm->done = 0;
    jm->bsa = bsa_create((void *) p, host_buffer_size, user_buffer_size,
        input_channels, output_channels, process_jm, jm);
    if (!jm->bsa) {
      free(jm);
      jm = NULL;
    }
  }
  return (jlong) jm;
}

JNIEXPORT void JNICALL
Java_com_noisepages_nettoyeur_patchfield_java_JavaModule_release
(JNIEnv *env, jobject obj, jlong p) {
  jmodule *jm = (jmodule *) p;
  bsa_release(jm->bsa);
  sem_destroy(&jm->wake);
  sem_destroy(&jm->ready);
  free(jm);
}

JNIEXPORT void JNICALL
Java_com_noisepages_nettoyeur_patchfield_java_JavaModule_fillInputBuffer
(JNIEnv *env, jobject obj, jlong p, jfloatArray buffer) {
  jmodule *jm = (jmodule *) p;
  sem_post(&jm->wake);
  if (!jm->done) {
    int n = (*env)->GetArrayLength(env, buffer);
    float *b = (*env)->GetFloatArrayElements(env, buffer, NULL);
    memcpy(b, jm->input_buffer, n * sizeof(float));
    (*env)->ReleaseFloatArrayElements(env, buffer, b, 0);
  }
}

JNIEXPORT void JNICALL
Java_com_noisepages_nettoyeur_patchfield_java_JavaModule_sendOutputBuffer
(JNIEnv *env, jobject obj, jlong p, jfloatArray buffer) {
  jmodule *jm = (jmodule *) p;
  if (!jm->done) {
    int n = (*env)->GetArrayLength(env, buffer);
    float *b = (*env)->GetFloatArrayElements(env, buffer, NULL);
    memcpy(jm->output_buffer, b, n * sizeof(float));
    (*env)->ReleaseFloatArrayElements(env, buffer, b, 0);
    sem_post(&jm->ready);
  }
}

JNIEXPORT void JNICALL
Java_com_noisepages_nettoyeur_patchfield_java_JavaModule_signalThread
(JNIEnv *env, jobject obj, jlong p) {
  jmodule *jm = (jmodule *) p;
  jm->done = 1;
  sem_post(&jm->wake);
}

