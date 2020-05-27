package com.dfrobot.angelo.blunobasicdemo;

// Cleanup

import android.Manifest;
import android.app.AlertDialog;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.content.Intent;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;

public class MainActivity extends BlunoLibrary {
    private Button buttonScan;
    private EditText serialSendText;
    private TextView serialReceivedText;
    private SeekBar seekBar1;
    private SeekBar seekBar2;
    private TextView textseekb1;
    private TextView textseekb2;
    private Button calbutton;
    private Button execbutton;

    private static String all = " ";
    private int lpos = 0;
    private int rpos = 0;
    private int oldlpos = -1;
    private int oldrpos = -1;

    boolean initpos = true;

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
        seekBar1.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                textseekb1.setText("L " + i);
                lpos = i;
                // serialSend("pl"+i+"\r");
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {

            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {

            }
        });
        seekBar2 = findViewById(R.id.seekBar2);
        seekBar2.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int i, boolean b) {
                textseekb2.setText("R " + i);
                rpos = i;
                // serialSend("pr"+i+"\r");
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

        calbutton = findViewById(R.id.calbutton);

        execbutton = findViewById(R.id.execbutton);


    }

    public void calclick(View view) {
        serialSend("q100" + "\r");
    }


    public void execlick(View view) {
        // initial click gets current position
        if (initpos) {
            serialSend("e\r");
        } else {
            //Log.w("YYYY", "all " + all);
            if (lpos != oldlpos) {
                serialSend("pl" + lpos + "\r");
                oldlpos = lpos;
            }
            if (rpos != oldrpos) {
                serialSend("pr" + rpos + "\r");
                oldrpos = rpos;
            }
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
    public void onConectionStateChange(connectionStateEnum theConnectionState) {
        switch (theConnectionState) {
            case isConnected:
                buttonScan.setText(R.string.connected);  // Naam device van maken
                break;
            case isConnecting:
                buttonScan.setText(R.string.connecting);
                break;
            case isToScan:
                buttonScan.setText(R.string.scan);
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
        String temp;
        all = all + theString;

        if (initpos) {
            // lees Enc: i,j X
            if (all.indexOf('X') > 0) {
                // should contain: Enc: x,yX
                temp = all.replace("Enc: ", "");
                String tempp = temp.replace("X", "");
                tempp = tempp.replaceAll("\\s+","");
                String[] temp2 = tempp.split(",");
                int l = (int)Integer.parseInt(temp2[0])/250;
                int r = (int)Integer.parseInt(temp2[1])/250;
                textseekb1.setText("L " + l);
                textseekb2.setText("R " + r);
                initpos = false;
                execbutton.setText("Execute");
            }
        }

        serialReceivedText.setText(all);
        //The Serial data from the BLUNO may be sub-packaged, so using a buffer to hold the String is a good choice.
        ((ScrollView) serialReceivedText.getParent()).fullScroll(View.FOCUS_DOWN);
    }


}