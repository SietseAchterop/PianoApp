package com.dfrobot.angelo.pianoapp;

// Cleanup

import android.Manifest;
import android.app.AlertDialog;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.os.Bundle;
import android.content.Intent;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;

import java.lang.Math;

public class MainActivity extends BlunoLibrary {
    private Button buttonScan;
    private EditText serialSendText;
    private TextView serialReceivedText;
    private TextView textseekb1;
    private TextView textseekb2;
    private Button execbutton;
    private Button accubutton;
    private Button calbutton;

    private static String all = " ";
    private static String combuf = " ";
    private int lpos = 0;
    private int rpos = 0;
    private int oldlpos = 0;
    private int oldrpos = 0;
    SeekBar seekBar1;
    SeekBar seekBar2;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        //onCreate Process by BlunoLibrary
        onCreateProcess();
        requestPermissions(new String[]{Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.ACCESS_FINE_LOCATION}, 1);
        //set the Uart Baudrate on BLE chip to 115200
        serialBegin(115200);

        serialReceivedText = (TextView) findViewById(R.id.serialReceivedText);
        serialSendText = (EditText) findViewById(R.id.serialSendText);
        Button buttonSerialSend = (Button) findViewById(R.id.buttonSerialSend);
        buttonSerialSend.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                String str = serialSendText.getText().toString() + "\r";   // "\r" to end the command
                serialSendText.setText("");
                serialSend(str);
            }
        });

        buttonScan = (Button) findViewById(R.id.buttonScan);
        buttonScan.setOnClickListener(new OnClickListener() {

            @Override
            public void onClick(View v) {
                // TODO Auto-generated method stub
                int permissionCheck = ContextCompat.checkSelfPermission(getApplicationContext(), Manifest.permission.ACCESS_FINE_LOCATION);
                if (permissionCheck != PackageManager.PERMISSION_GRANTED) {
                    boolean requestCheck = ActivityCompat.shouldShowRequestPermissionRationale(MainActivity.this, Manifest.permission.ACCESS_FINE_LOCATION);
                    if (requestCheck) {
                        requestPermissions(new String[]{Manifest.permission.ACCESS_COARSE_LOCATION, Manifest.permission.ACCESS_FINE_LOCATION}, 1);
                    } else {
                        new AlertDialog.Builder(MainActivity.this)
                                .setTitle("Permission Required")
                                .setMessage("Please enable location permission to use this application.")
                                .setNeutralButton("I Understand", null)
                                .show();
                    }
                } else {
                    buttonScanOnClickProcess(); //Alert Dialog for selecting the BLE device
                }
            }
        });

        seekBar1 = findViewById(R.id.seekBar1);
        seekBar1.getProgressDrawable().setColorFilter(Color.BLUE, PorterDuff.Mode.SRC_IN);
        seekBar2 = findViewById(R.id.seekBar2);
        seekBar2.getProgressDrawable().setColorFilter(Color.BLUE, PorterDuff.Mode.SRC_IN);
        seekBar1.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                lpos = i - 5;
                textseekb1.setText("L " + lpos);
                if (lpos != oldlpos) {
                    seekBar1.getProgressDrawable().setColorFilter(Color.RED, PorterDuff.Mode.SRC_IN);
                }
                else {
                    seekBar1.getProgressDrawable().setColorFilter(Color.BLUE, PorterDuff.Mode.SRC_IN);
                }

                // serialSend("pl"+ lpos +"\r");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });

        seekBar2.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                rpos = i - 5;
                textseekb2.setText("R " + rpos);
                if (rpos != oldrpos) {
                    seekBar2.getProgressDrawable().setColorFilter(Color.RED, PorterDuff.Mode.SRC_IN);
                }
                else {
                    seekBar2.getProgressDrawable().setColorFilter(Color.BLUE, PorterDuff.Mode.SRC_IN);
                }

                // serialSend("pr"+rpos+"\r");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });
        textseekb1 = findViewById(R.id.textseekb1);
        textseekb2 = findViewById(R.id.textseekb2);

        execbutton = findViewById(R.id.execbutton);
        accubutton = findViewById(R.id.accubutton);
        calbutton  = findViewById(R.id.calbutton);
    }

    public void calclick(View view) {
        serialSend("q150" + "\r");
    }


    public void execlick(View view) {
        // initial click gets current position
        boolean dirty = false;
        //Log.w("YYYY", "all " + all);
        // ook als er geen verandereing is? Robuster?
        if (lpos != oldlpos) {
            serialSend("pl" + lpos + "\r");
            oldlpos = lpos;
            dirty = true;
        }
        if (rpos != oldrpos) {
            serialSend("pr" + rpos + "\r");
            oldrpos = rpos;
            dirty = true;
        }
        if (dirty) {  // save to EEPROM
            serialSend("X0\r");
        }
    }

    protected void onResume() {
        super.onResume();
        System.out.println("BlUNOActivity onResume");
        onResumeProcess();
    }


    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        onActivityResultProcess(requestCode, resultCode, data);
        super.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    protected void onPause() {
        super.onPause();
        onPauseProcess();
    }

    protected void onStop() {
        super.onStop();
        onStopProcess();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        onDestroyProcess();
    }

    @Override
    public void onConnectionStateChange(connectionStateEnum theConnectionState) {
        switch (theConnectionState) {
            case isConnected:
                buttonScan.setText(R.string.connected);  // Naam device van maken
                buttonScan.setTextColor(Color.BLUE);
                serialSend("e\r");
                break;
            case isConnecting:
                buttonScan.setText(R.string.connecting);
                break;
            case isToScan:
                buttonScan.setText(R.string.scan);
                buttonScan.setTextColor(Color.WHITE);
                break;
            case isScanning:
                buttonScan.setText(R.string.scanning);
                break;
            case isDisconnecting:
                buttonScan.setText(R.string.isdisconnecting);
                break;
            default:
                break;
        }
    }

    @Override
    public void onSerialReceived(String theString) {
        String temp, command = "";
        combuf = combuf + theString;

        int i = combuf.indexOf('\r');
        while (i > 0) {
            command = combuf.substring(0, i + 1);
            combuf = combuf.substring(i + 1);
            all = all + command;
            // process command
            command = command.replaceAll("\\s+", "");
            String[] tmp = command.split("[:,]");

            if (command.contains("Enc:")) {
                lpos = (int)Math.round(Float.parseFloat(tmp[1])/2000.0);
                rpos = (int)Math.round(Float.parseFloat(tmp[2])/2000.0);
                System.out.println("=============" + tmp[0] + "  " + lpos + " " + rpos);
                seekBar1.setProgress(lpos+5);
                seekBar2.setProgress(rpos+5);
                oldlpos = lpos;
                oldrpos = rpos;
                seekBar1.getProgressDrawable().setColorFilter(Color.BLUE, PorterDuff.Mode.SRC_IN);
                seekBar2.getProgressDrawable().setColorFilter(Color.BLUE, PorterDuff.Mode.SRC_IN);
                textseekb1.setText("L " + lpos);
                textseekb2.setText("R " + rpos);
                execbutton.setTextColor(Color.WHITE);
                calbutton.setTextColor(Color.WHITE);
            } else if (command.contains("V:")) {
                float v = (float)(Integer.parseInt(tmp[1]))/1000;
                if (v <= 10.4) {
                    accubutton.setTextColor(Color.RED);
                } else {
                    accubutton.setTextColor(Color.GREEN);
                }
                String sp = String.format("%.1f Volt", v);
                accubutton.setText(sp);
            } else if (command.contains("Pfailed")) {
                execbutton.setTextColor(Color.RED);
            } else if (command.contains("Calfailed")) {
                calbutton.setTextColor(Color.RED);
            }

            System.out.println("Command: " + command);
            i = combuf.indexOf('\r');
        }

        serialReceivedText.setText(all);
        //The Serial data from the BLUNO may be sub-packaged, so using a buffer to hold the String is a good choice.
        ((ScrollView) serialReceivedText.getParent()).fullScroll(View.FOCUS_DOWN);
    }
}
