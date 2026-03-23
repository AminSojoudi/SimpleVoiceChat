using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
#if UNITY_ANDROID && !UNITY_EDITOR
using UnityEngine.Android;
#endif

/// <summary>
/// Native voice chat client (GameNetworkingSockets). Place on a GameObject with an AudioSource; add an AudioListener in the scene for playback via OnAudioFilterRead.
/// Windows: VoiceChatClientPlugin.dll in Assets/Plugins/x86_64 (or ARM64). Android: libVoiceChatClientPlugin.so per ABI. iOS: link libVoiceChatClientPlugin.a and GNS/OpenSSL/protobuf static libs with DllImport("__Internal").
/// </summary>
public class VoiceChatUnityClient : MonoBehaviour {
#if UNITY_IOS && !UNITY_EDITOR
    private const string PluginName = "__Internal";
#else
    private const string PluginName = "VoiceChatClientPlugin";
#endif

    [DllImport(PluginName)]
    private static extern bool VC_Init();

    [DllImport(PluginName)]
    private static extern bool VC_Connect(string serverIp, ushort port, long channel);

    [DllImport(PluginName)]
    private static extern void VC_Tick();

    [DllImport(PluginName)]
    private static extern bool VC_SendAudio(short[] samples, int sampleCount);

    [DllImport(PluginName)]
    private static extern int VC_RecvAudio(short[] outSamples, int maxSamples);

    [DllImport(PluginName)]
    private static extern void VC_Shutdown();

    public string serverAddress = "192.168.1.100";
    public ushort serverPort = 27020;
    public long channel = 0;

    public bool autoStart = true;
    public int sampleRate = 44100;
    public int frameSize = 256;

    private bool _nativeReady;
    private bool _sessionActive;
    private bool _cleanedUp;
#if UNITY_ANDROID && !UNITY_EDITOR
    private bool _micPermissionRequested;
#endif

    private AudioClip micClip;
    private int micPosition = 0;
    private readonly Queue<float> playbackQueue = new Queue<float>(8192);
    private readonly object queueLock = new object();

    void Start() {
        Application.runInBackground = true;
        _nativeReady = VC_Init();
        if (!_nativeReady) {
            Debug.LogError("VoiceChat: VC_Init failed");
            return;
        }

        if (autoStart) {
            TryStartSession();
        }
    }

    void Update() {
        if (!_nativeReady)
            return;

#if UNITY_ANDROID && !UNITY_EDITOR
        if (autoStart && !_sessionActive && micClip == null)
            TryStartSession();
#endif

        if (micClip != null && Microphone.IsRecording(null)) {
            int pos = Microphone.GetPosition(null);
            if (pos >= 0 && pos != micPosition) {
                int diff = pos - micPosition;
                if (diff < 0) diff += micClip.samples;
                float[] raw = new float[diff];
                micClip.GetData(raw, micPosition);
                micPosition = pos;

                var sendBuffer = new List<short>(diff);
                for (int i = 0; i < raw.Length; i++) {
                    float v = Mathf.Clamp(raw[i], -1f, 1f);
                    sendBuffer.Add((short)(v * short.MaxValue));
                }
                if (sendBuffer.Count > 0)
                    VC_SendAudio(sendBuffer.ToArray(), sendBuffer.Count);
            }
        }

        VC_Tick();

        const int recvBlock = 1024;
        var recvSamples = new short[recvBlock];
        int received = VC_RecvAudio(recvSamples, recvBlock);
        if (received > 0) {
            lock (queueLock) {
                for (int i = 0; i < received; i++)
                    playbackQueue.Enqueue(recvSamples[i] / (float)short.MaxValue);
            }
        }
    }

    /// <summary>Connects to the server and starts microphone capture (after runtime mic permission on Android).</summary>
    public void StartClient() {
        TryStartSession();
    }

    private void TryStartSession() {
        if (!_nativeReady || _sessionActive)
            return;

#if UNITY_ANDROID && !UNITY_EDITOR
        if (!Permission.HasUserAuthorizedPermission(Permission.Microphone)) {
            if (!_micPermissionRequested) {
                Permission.RequestUserPermission(Permission.Microphone);
                _micPermissionRequested = true;
            }
            return;
        }
#endif

        if (!VC_Connect(serverAddress, serverPort, channel)) {
            Debug.LogError("VoiceChat: failed to connect");
            return;
        }

        if (Microphone.devices.Length == 0) {
            Debug.LogError("VoiceChat: no microphone found");
            return;
        }

        micClip = Microphone.Start(null, true, 1, sampleRate);
        while (!(Microphone.GetPosition(null) > 0)) { }
        micPosition = Microphone.GetPosition(null);
        _sessionActive = true;
        Debug.Log("VoiceChat: microphone started");
    }

    void OnAudioFilterRead(float[] data, int channels) {
        lock (queueLock) {
            for (int i = 0; i < data.Length; i++) {
                if (playbackQueue.Count > 0)
                    data[i] = playbackQueue.Dequeue();
                else
                    data[i] = 0f;
            }
        }
    }

    void OnApplicationQuit() {
        Cleanup();
    }

    void OnDisable() {
        Cleanup();
    }

    private void Cleanup() {
        if (_cleanedUp)
            return;
        _cleanedUp = true;

        if (Microphone.IsRecording(null))
            Microphone.End(null);

        micClip = null;
        _sessionActive = false;
#if UNITY_ANDROID && !UNITY_EDITOR
        _micPermissionRequested = false;
#endif
        VC_Shutdown();
        _nativeReady = false;
    }
}
