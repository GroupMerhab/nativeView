package com.nativeview.bridge;

import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class SandboxPathTest {

    @Rule public TemporaryFolder tmp = new TemporaryFolder();

    @Test
    public void resolveUnderBase_relativeOk() throws IOException {
        File base = tmp.newFolder("base");
        File f = SandboxPath.resolveUnderBase(base, "a/b.txt");
        assertTrue(f.getPath().endsWith("a" + File.separator + "b.txt"));
    }

    @Test(expected = SecurityException.class)
    public void resolveUnderBase_rejectsDotDot() throws IOException {
        File base = tmp.newFolder("base2");
        SandboxPath.resolveUnderBase(base, "a/../secret");
    }

    @Test(expected = SecurityException.class)
    public void resolveUnderBase_rejectsEncodedDotDot() throws IOException {
        File base = tmp.newFolder("base3");
        SandboxPath.resolveUnderBase(base, "a/%2e%2e/etc");
    }

    @Test(expected = SecurityException.class)
    public void resolveUnderBase_rejectsAbsolute() throws IOException {
        File base = tmp.newFolder("base4");
        SandboxPath.resolveUnderBase(base, "/etc/passwd");
    }

    @Test
    public void resolveUnderBase_rejectsEscapeViaSymlink() throws IOException {
        File base = tmp.newFolder("base5");
        File outside = tmp.newFolder("outside");
        File linkTarget = new File(base, "link");
        try {
            java.nio.file.Files.createSymbolicLink(
                    linkTarget.toPath(), outside.toPath());
        } catch (UnsupportedOperationException | IOException ex) {
            return;
        }
        try {
            SandboxPath.resolveUnderBase(base, "link/../evil");
            Assert.fail("expected SecurityException");
        } catch (SecurityException expected) {
            // ok
        }
    }
}
