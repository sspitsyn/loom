/*
 * Copyright (c) 2003, 2022, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 *
 * @summary converted from VM Testbase nsk/jvmti/FramePop/framepop001.
 * VM Testbase keywords: [quick, jpda, jvmti, noras]
 * VM Testbase readme:
 * DESCRIPTION
 *     The test exercises JVMTI event callback function FramePop.
 *     The test checks the following:
 *       - if clazz, method and frame parameters contain expected values
 *         for event generated upon exit from single method in single frame
 *         specified in call to NotifyFramePop.
 *       - if GetFrameLocation identifies the executable location
 *         in the returning method, immediately prior to the return.
 * COMMENTS
 *     Ported from JVMDI.
 *
 * @library /test/lib
 * @compile --enable-preview -source ${jdk.version} framepop01a.java
 * @run main/othervm/native --enable-preview -agentlib:framepop01 framepop01
 */

public class framepop01 {

    static {
        try {
            System.loadLibrary("framepop01");
        } catch (UnsatisfiedLinkError ule) {
            System.err.println("Could not load framepop01 library");
            System.err.println("java.library.path:"
                + System.getProperty("java.library.path"));
            throw ule;
        }
    }

    static volatile int result;
    native static int check();

    public static void main(String args[]) {
        testVirtual();
        testKernel();
    }
    public static void testVirtual() {
        Thread thread = Thread.startVirtualThread(() -> {
            result = check();
        });
        try {
            thread.join();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }

        if (result != 0) {
            throw new RuntimeException("check failed with result " + result);
        }
    }
    public static void testKernel() {
        result = check();
        if (result != 0) {
            throw new RuntimeException("check failed with result " + result);
        }
    }

    public static void chain() {
    }
}
