package com.example.llenadorabt;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Set;
import java.util.UUID;

public class MainActivity extends Activity {
    private static final int REQ_BT = 21;
    private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");
    private BluetoothAdapter adapter;
    private final ArrayList<BluetoothDevice> devices = new ArrayList<>();
    private ArrayAdapter<String> deviceNames;
    private Spinner deviceSpinner;
    private TextView connection, state, sensor, motor, servo, boxes, speedDirection, log;
    private EditText fillTime, speed, degrees;
    private BluetoothSocket socket;
    private OutputStream output;
    private volatile boolean reading = false;

    @Override public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        adapter = BluetoothAdapter.getDefaultAdapter();
        buildUi();
        if (adapter == null) { connection.setText("Este teléfono no tiene Bluetooth"); return; }
        requestBluetoothPermission();
    }

    private void buildUi() {
        ScrollView scroll = new ScrollView(this);
        LinearLayout page = new LinearLayout(this); page.setOrientation(LinearLayout.VERTICAL); page.setPadding(dp(18), dp(18), dp(18), dp(24));
        page.setBackgroundColor(0xfff2f6f4); scroll.addView(page);
        TextView title = text("Control de llenadora", 24, 0xff153b36); page.addView(title);
        connection = text("Bluetooth desconectado", 15, 0xff58716a); page.addView(connection);
        deviceSpinner = new Spinner(this); deviceNames = new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, new ArrayList<>()); deviceSpinner.setAdapter(deviceNames); page.addView(deviceSpinner);
        row(page, button("Buscar dispositivos emparejados", v -> loadPaired()));
        row(page, button("Conectar", v -> connect()));
        state = text("Proceso: —", 20, 0xff18312d); page.addView(state);
        sensor = text("Sensor: —", 17, 0xff18312d); page.addView(sensor);
        motor = text("Banda: —", 17, 0xff18312d); page.addView(motor);
        servo = text("Embudo: —", 17, 0xff18312d); page.addView(servo);
        boxes = text("Cajitas llenadas: 0", 17, 0xff18312d); page.addView(boxes);
        speedDirection = text("Velocidad: —", 17, 0xff18312d); page.addView(speedDirection);
        row(page, button("INICIAR AUTOMÁTICO", v -> send("INICIAR")), button("PARAR TODO", v -> send("PARAR")));
        row(page, button("Giro derecha", v -> send("DERECHA")), button("Giro izquierda", v -> send("IZQUIERDA")));
        fillTime = input("Tiempo llenado (s)", "2"); page.addView(fillTime);
        row(page, button("Guardar tiempo", v -> { if (valid(fillTime, .5, 10)) send("TIEMPO " + fillTime.getText()); }));
        speed = input("Velocidad (RPM)", "4.88"); page.addView(speed);
        row(page, button("Guardar velocidad", v -> { if (valid(speed, 1, 15)) send("VELOCIDAD " + speed.getText()); }));
        degrees = input("Movimiento manual (grados)", "90"); page.addView(degrees);
        row(page, button("Mover grados", v -> { if (valid(degrees, 1, 3600)) send("GRADOS " + degrees.getText()); }));
        log = text("Conecta primero con LlenadoraESP32.", 13, 0xff58716a); page.addView(log);
        setContentView(scroll);
    }

    private void requestBluetoothPermission() {
        if (Build.VERSION.SDK_INT >= 31 && checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{Manifest.permission.BLUETOOTH_CONNECT}, REQ_BT);
        } else loadPaired();
    }

    @Override public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (requestCode == REQ_BT && results.length > 0 && results[0] == PackageManager.PERMISSION_GRANTED) loadPaired();
        else connection.setText("Se necesita permiso de Bluetooth para controlar el ESP32");
    }

    private void loadPaired() {
        if (adapter == null) return;
        try {
            if (!adapter.isEnabled()) { connection.setText("Activa Bluetooth en los ajustes del teléfono"); return; }
            Set<BluetoothDevice> paired = adapter.getBondedDevices(); devices.clear(); deviceNames.clear();
            for (BluetoothDevice d : paired) { devices.add(d); deviceNames.add(d.getName() + "  ·  " + d.getAddress()); }
            if (devices.isEmpty()) connection.setText("Empareja LlenadoraESP32 en Ajustes > Bluetooth y vuelve aquí");
            else connection.setText("Selecciona LlenadoraESP32 y pulsa Conectar");
        } catch (SecurityException e) { connection.setText("Permiso de Bluetooth denegado"); }
    }

    private void connect() {
        if (devices.isEmpty()) { loadPaired(); return; }
        final BluetoothDevice selected = devices.get(deviceSpinner.getSelectedItemPosition());
        connection.setText("Conectando con " + selected.getName() + "…");
        new Thread(() -> {
            try {
                if (socket != null) try { socket.close(); } catch (Exception ignored) {}
                BluetoothSocket candidate = selected.createRfcommSocketToServiceRecord(SPP_UUID);
                candidate.connect(); socket = candidate; output = candidate.getOutputStream();
                reading = true;
                runOnUiThread(() -> { connection.setText("Conectado por Bluetooth"); addLog("Conectado. Telemetría en vivo activa."); });
                BufferedReader reader = new BufferedReader(new InputStreamReader(candidate.getInputStream()));
                String line;
                while (reading && (line = reader.readLine()) != null) {
                    final String message = line;
                    runOnUiThread(() -> handleLine(message));
                }
            } catch (Exception e) {
                runOnUiThread(() -> { connection.setText("No conectado. Verifica emparejamiento y vuelve a intentar."); addLog(e.getMessage() == null ? "Error Bluetooth" : e.getMessage()); });
            } finally { reading = false; }
        }).start();
    }

    private void handleLine(String line) {
        if (line.startsWith("STATUS ")) {
            try {
                JSONObject j = new JSONObject(line.substring(7));
                state.setText("Proceso: " + j.optString("estado"));
                sensor.setText("Sensor: " + (j.optBoolean("sensor") ? "CAJITA DETECTADA" : "Libre"));
                motor.setText("Banda: " + (j.optBoolean("motor") ? "Activa" : "Detenida"));
                servo.setText("Embudo: " + (j.optBoolean("servoAbierto") ? "Abierto" : "Cerrado"));
                boxes.setText("Cajitas llenadas: " + j.optInt("cajas"));
                speedDirection.setText("Velocidad: " + j.optDouble("velocidad") + " RPM · " + j.optString("direccion"));
                if (!fillTime.hasFocus()) fillTime.setText(String.valueOf(j.optDouble("tiempo")));
                if (!speed.hasFocus()) speed.setText(String.valueOf(j.optDouble("velocidad")));
            } catch (Exception ignored) { addLog("No se pudo leer estado del ESP32"); }
        } else if (!line.isEmpty()) addLog(line);
    }

    private void send(String command) {
        try {
            if (output == null || socket == null || !socket.isConnected()) { toast("Conecta primero por Bluetooth"); return; }
            output.write((command + "\n").getBytes("UTF-8")); output.flush(); addLog("Enviado: " + command);
        } catch (Exception e) { toast("Error al enviar el comando"); }
    }

    private boolean valid(EditText field, double min, double max) {
        try { double n = Double.parseDouble(field.getText().toString()); if (n >= min && n <= max) return true; } catch (Exception ignored) {}
        toast("Valor permitido: " + min + " a " + max); return false;
    }
    private EditText input(String hint, String initial) { EditText e = new EditText(this); e.setSingleLine(true); e.setHint(hint); e.setText(initial); e.setInputType(8194); e.setPadding(dp(12), dp(8), dp(12), dp(8)); return e; }
    private TextView text(String value, int size, int color) { TextView t = new TextView(this); t.setText(value); t.setTextSize(size); t.setTextColor(color); t.setPadding(0, dp(6), 0, dp(6)); return t; }
    private Button button(String label, View.OnClickListener click) { Button b = new Button(this); b.setText(label); b.setOnClickListener(click); return b; }
    private void row(LinearLayout parent, Button... buttons) { LinearLayout r = new LinearLayout(this); r.setGravity(Gravity.CENTER_VERTICAL); for (Button b : buttons) { LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, -2, 1); p.setMargins(dp(3), dp(2), dp(3), dp(2)); r.addView(b, p); } parent.addView(r); }
    private int dp(int n) { return (int)(n * getResources().getDisplayMetrics().density + .5f); }
    private void addLog(String s) { log.setText(s); }
    private void toast(String s) { Toast.makeText(this, s, Toast.LENGTH_SHORT).show(); }

    @Override protected void onDestroy() {
        reading = false;
        try { if (socket != null) socket.close(); } catch (Exception ignored) {}
        super.onDestroy();
    }
}
