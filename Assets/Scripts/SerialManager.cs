using UnityEngine;
using System.IO.Ports;
using System.Collections;

public class SerialManager : MonoBehaviour 
{
    private static SerialManager instance;
    public static SerialManager Instance { get { return instance; } }

    private SerialPort port;

    [SerializeField] private string portName = "COM3";  // "/dev/ttyUSB0" for Linux etc.
    [SerializeField] private int baudRate = 9600;
    
    [SerializeField] private GameMenu gameMenu;
    [SerializeField] private BoardVisualizer boardVisualizer;
    
    private int pendingShips  = -1;
    private int pendingAttacks = -1;

    private void Awake() {
        if (instance == null) { 
            instance = this; 
            DontDestroyOnLoad(gameObject);
        } else { 
            Destroy(gameObject); 
        }
    }

    private void Start() {
        OpenPort();
    }

    void OpenPort() {
        try {
            port = new SerialPort(portName, baudRate);
            port.ReadTimeout = 500;
            port.Open();
            Debug.Log("Port opened on " + portName);
        }
        catch (System.Exception e) {
            Debug.LogError("Could not open the port: " + e.Message);
        }
    }

    private void Update() {
        if (port != null && port.IsOpen) {
            try {
                string line = port.ReadLine();
                if (!string.IsNullOrEmpty(line)) {
                    ParseArduinoMessage(line.Trim());
                }
            }
            catch (System.TimeoutException) {
            }
        }
    }
    
    public void SendMessageToArduino(string msg) {
        if (port != null && port.IsOpen) {
            port.WriteLine(msg);
            Debug.Log("[Unity->Arduino] " + msg);
        } else {
            Debug.LogWarning("Port not open, can't send: " + msg);
        }
    }
    
    void ParseArduinoMessage(string line) 
    {
        Debug.Log("[Arduino->Unity] " + line);
        
        if (line.StartsWith("TOTAL_SHIPS="))
        {
            string valueStr = line.Substring("TOTAL_SHIPS=".Length);
            if (int.TryParse(valueStr, out int ships))
            {
                pendingShips = ships;
                if (pendingAttacks > -1)
                {
                    gameMenu.InitializeGame(pendingAttacks, pendingShips);
                    pendingAttacks = -1;
                    pendingShips   = -1;
                }
            }
        }
        else if (line.StartsWith("TOTAL_ATTACKS="))
        {
            string valueStr = line.Substring("TOTAL_ATTACKS=".Length);
            if (int.TryParse(valueStr, out int attacks))
            {
                pendingAttacks = attacks;
                if (pendingShips > -1)
                {
                    gameMenu.InitializeGame(pendingAttacks, pendingShips);
                    pendingAttacks = -1;
                    pendingShips   = -1;
                }
            }
        }
        else if (line == "ATTACK")
        {
            gameMenu.OnAttack();
        }
        else if (line == "SHIP_DESTROYED")
        {
            gameMenu.OnShipDestroyed();
        }
        else if (line == "WIN")
        {
            gameMenu.OnWin();
        }
        else if (line == "LOSE")
        {
            gameMenu.OnLose();
        }
        else if (line.StartsWith("MISS ") ||
                 line.StartsWith("HIT ")  ||
                 line.StartsWith("KILL ") ||
                 line.StartsWith("REVEAL "))
        {
            string[] parts = line.Split(' ');
            if (parts.Length < 3) return;

            string cmd = parts[0];
            int x = int.Parse(parts[1]);
            int y = int.Parse(parts[2]);
            
            Color c = Color.gray;
            if (cmd == "MISS") c = Color.red;
            else if (cmd == "HIT") c = Color.green;
            else if (cmd == "KILL") c = Color.blue;
            else if (cmd == "REVEAL") c = new Color(0.5f,0f,0.5f);
            
            boardVisualizer.SetCellColor(x, y, c);
        }
        else if (line == "RESET")
        {
            boardVisualizer.ResetBoard();
        }
        else {
            Debug.LogWarning("Unknown message from Arduino: " + line);
        }
    }

    private void OnDestroy() {
        if (port != null && port.IsOpen) {
            port.Close();
        }
    }
}
