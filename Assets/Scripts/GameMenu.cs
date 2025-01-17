using UnityEngine;
using TMPro;

public class GameMenu : MonoBehaviour
{
    [Header("UI References")]
    public GameObject mainPanel;

    [SerializeField] private TextMeshProUGUI timerText;
    [SerializeField] private TextMeshProUGUI totalAttacksText;
    [SerializeField] private TextMeshProUGUI totalShipsText;
    [SerializeField] private TextMeshProUGUI attacksLeftText;
    [SerializeField] private TextMeshProUGUI shipsDestroyedText;

    private float elapsedTime;
    private bool  isTimerRunning;

    private int totalAttacks;
    private int totalShips;
    private int attacksLeft;
    private int shipsDestroyed;
    
    private void Update()
    {
        if (isTimerRunning)
        {
            elapsedTime += Time.deltaTime;
            int minutes = (int)(elapsedTime / 60f);
            int seconds = (int)(elapsedTime % 60f);
            timerText.text = string.Format("{0:00}:{1:00}", minutes, seconds);
        }
    }
    
    public void InitializeGame(int totalAttacksFromArduino, int totalShipsFromArduino)
    {
        totalAttacks   = totalAttacksFromArduino;
        totalShips     = totalShipsFromArduino;
        attacksLeft    = totalAttacks; 
        shipsDestroyed = 0;
        
        totalAttacksText.text    = totalAttacks.ToString();
        totalShipsText.text      = totalShips.ToString();
        attacksLeftText.text     = attacksLeft.ToString();
        shipsDestroyedText.text  = shipsDestroyed.ToString();
        
        elapsedTime = 0f;
        isTimerRunning = true;
    }
    
    public void StartTimerNow()
    {
        elapsedTime = 0f;     
        isTimerRunning = true;
    }

    
    public void OnAttack()
    {
        attacksLeft--;
        if (attacksLeft < 0) attacksLeft = 0;
        attacksLeftText.text = attacksLeft.ToString();
    }
    
    public void OnShipDestroyed()
    {
        shipsDestroyed++;
        shipsDestroyedText.text = shipsDestroyed.ToString();
    }
    
    
    public void OnWin()
    {
        isTimerRunning = false;
        Debug.Log("You Win!");
    }
    
    public void OnLose()
    {
        isTimerRunning = false;
        Debug.Log("You Lose!");
    }
    
    public void ResetGame()
    {
        isTimerRunning = false;
        this.gameObject.SetActive(false);
        mainPanel.SetActive(true);
    }
}
