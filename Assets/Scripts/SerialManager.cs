using UnityEngine;
using System.IO.Ports;
using System.Collections.Concurrent;
using System.Threading;

public class SerialManager : MonoBehaviour 
{
    private static SerialManager instance;
    public static SerialManager Instance { get { return instance; } }

    private SerialPort port;

    [SerializeField] private string portName = "COM3";  // Exemplu: "/dev/ttyUSB0" pe Linux
    [SerializeField] private int baudRate = 9600;
    
    [SerializeField] private GameMenu gameMenu;
    [SerializeField] private BoardVisualizer boardVisualizer;
    
    private int pendingShips  = -1;
    private int pendingAttacks = -1;
    
    private Thread readThread;
    private bool isReading = false;
    
    private ConcurrentQueue<string> messageQueue = new ConcurrentQueue<string>();

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
            
            isReading = true;
            readThread = new Thread(ReadSerialLoop);
            readThread.Start();
        }
        catch (System.Exception e) {
            Debug.LogError("Could not open the port: " + e.Message);
        }
    }
    
    private void ReadSerialLoop() {
        while (isReading && port != null && port.IsOpen) {
            try {
                string line = port.ReadLine();
                if (!string.IsNullOrEmpty(line)) {
                    messageQueue.Enqueue(line);
                }
            }
            catch (System.TimeoutException) {
            }
            catch (System.Exception e) {
                Debug.LogError("Error reading from port: " + e.Message);
            }
        }
    }

    private void Update() {
        while (messageQueue.TryDequeue(out string line)) {
            ParseArduinoMessage(line.Trim());
        }
    }
    
    public void SendMessageToArduino(string msg) {
        if (port != null && port.IsOpen) {
            port.WriteLine(msg);
        } else {
            Debug.LogWarning("Port not open, can't send: " + msg);
        }
    }
    
    void ParseArduinoMessage(string line) 
    {
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
            else if (cmd == "REVEAL") c = new Color(0.5f, 0f, 0.5f);
            
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
        isReading = false;
        if (readThread != null && readThread.IsAlive) {
            readThread.Join();
        }

        if (port != null && port.IsOpen) {
            port.Close();
        }
    }
}
