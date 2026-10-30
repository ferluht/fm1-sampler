package com.ferluht.fm1sampler;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.view.WindowManager;
import java.io.File;

/** The app: starts the backend (engine, FM-1 link, audio), shows the sampler and opens the system file picker when the UI asks for a file. */
public class SamplerActivity extends Activity {
    static final int PICK = 7;
    Backend backend;

    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().setStatusBarColor(0xff14141a);
        getWindow().setNavigationBarColor(0xff14141a);
        backend = Backend.get(this);
        backend.onFileRequest = this::pickFile;
        backend.start();
        SamplerView v = new SamplerView(this, backend.engine, 6, 6);
        v.setFitsSystemWindows(true);
        setContentView(v);
        if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) backend.startMic();
        else requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO}, 1);
    }

    void pickFile() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        i.putExtra(Intent.EXTRA_MIME_TYPES, new String[]{"audio/*", "application/ogg"});
        try { startActivityForResult(i, PICK); } catch (Exception e) { Native.say(backend.engine, "NO FILE PICKER ON THIS PHONE"); }
    }

    @Override protected void onActivityResult(int code, int result, Intent data) {
        super.onActivityResult(code, result, data);
        if (code != PICK || result != RESULT_OK || data == null || data.getData() == null) return;
        final Uri uri = data.getData();
        Native.say(backend.engine, "READING THE FILE...");
        new Thread(() -> {   // decoding can take a moment: off the UI thread
            try {
                File out = AudioImport.importTo(this, uri, getFilesDir());
                Native.importFile(backend.engine, out.getAbsolutePath());
            } catch (Exception e) {
                android.util.Log.w("fm1sampler", "import: " + e);
                Native.say(backend.engine, "CANNOT READ THIS FILE");
            }
        }, "import").start();
    }

    @Override public void onRequestPermissionsResult(int code, String[] perms, int[] res) {
        if (res.length > 0 && res[0] == PackageManager.PERMISSION_GRANTED) backend.startMic();
    }
}
