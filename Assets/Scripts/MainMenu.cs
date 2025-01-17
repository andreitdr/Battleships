using System;
using System.Collections;
using UnityEngine;

public class MainMenu : MonoBehaviour
{
    [Header("Panels")]
    public GameObject mainPanel;
    
    [SerializeField] private GameMenu gameMenu;
    
    public void DifficultyChanged(Single value)
    {
        int difficulty = Mathf.RoundToInt(value);
        string msg = "SET DIFFICULTY=" + difficulty;
        StartCoroutine(SendMessageToArduinoAsync(msg));
        Debug.Log("Sent: " + msg);
    }
    
    public void StartGame()
    {
        gameMenu.StartTimerNow();
        
        StartCoroutine(SendMessageToArduinoAsync("START"));
        Debug.Log("Sent: START");
        
        gameObject.SetActive(false);
        mainPanel.SetActive(true);
    }
    
    private IEnumerator SendMessageToArduinoAsync(string msg)
    {
        SerialManager.Instance.SendMessageToArduino(msg);
        yield return new WaitForEndOfFrame();
    }

    public void ExitGame()
    {
        Application.Quit();
    }
}