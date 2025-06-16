import React, { useState, useEffect } from 'react';
import { StyleSheet, Text, View, Button, ActivityIndicator, Alert } from 'react-native';
import firebase from 'firebase/app'; // Correct import for firebase/app
import 'firebase/database'; // Import Realtime Database module

// === Firebase Project Configuration ===
// IMPORTANT: Replace with YOUR Firebase Project Configuration
const firebaseConfig = {
  apiKey: "AIzaSyAWJh85grpimyf6qvLOQZq_VVNZZQHhSoI",
  authDomain: "electronic-click.firebaseapp.com",
  databaseURL: "https://electronic-click-default-rtdb.firebaseio.com",
  projectId: "electronic-click",
  storageBucket: "electronic-click.firebasestorage.app",
  messagingSenderId: "722012583887",
  appId: "1:722012583887:web:0aa0de51abef76dc21c4ba",
  measurementId: "G-TJ35T3TZMH"
};


// === Realtime Database Paths ===
const LED_COMMAND_PATH = "artifacts/default-app-id/public/data/led_rtdb_control/command";
const LED_STATE_PATH = "artifacts/default-app-id/public/data/led_rtdb_control/currentLedState";

// Initialize Firebase if not already initialized
if (!firebase.apps.length) {
  firebase.initializeApp(firebaseConfig);
}

const db = firebase.database(); // Get a reference to the database service

export default function App() {
  const [ledState, setLedState] = useState('unknown');
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    let ledStateRef;

    try {
      // Listener for the current LED state from the ESP32
      ledStateRef = db.ref(LED_STATE_PATH);
      ledStateRef.on('value', snapshot => {
        const state = snapshot.val();
        if (state) {
          setLedState(state);
        } else {
          setLedState('off'); // Default to off if no state exists
        }
        setLoading(false);
      }, (err) => {
        console.error("Firebase Realtime Database Listener Error: ", err);
        setError("Failed to listen for LED state. Check rules/path.");
        setLoading(false);
      });
    } catch (e) {
      console.error("Firebase Initialization/Listener Error: ", e);
      setError("Firebase setup failed. Check configuration.");
      setLoading(false);
    }

    // Cleanup listener on component unmount
    return () => {
      if (ledStateRef) {
        ledStateRef.off();
      }
    };
  }, []); // Run once on component mount

  const sendLedCommand = async (command) => {
    try {
      setLoading(true); // Show loading while sending command
      await db.ref(LED_COMMAND_PATH).set(command);
      Alert.alert("Command Sent", `LED command '${command}' sent successfully.`);
    } catch (e) {
      console.error("Error sending LED command: ", e);
      Alert.alert("Error", `Failed to send command: ${e.message}`);
    } finally {
      setLoading(false); // Hide loading
    }
  };

  if (loading) {
    return (
      <View style={styles.container}>
        <ActivityIndicator size="large" color="#0000ff" />
        <Text>Loading LED state...</Text>
      </View>
    );
  }

  if (error) {
    return (
      <View style={styles.container}>
        <Text style={styles.errorText}>Error: {error}</Text>
        <Text>Please check your Firebase configuration and network.</Text>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      <Text style={styles.title}>ESP32 LED Control</Text>
      <Text style={styles.stateText}>Current LED State: {ledState.toUpperCase()}</Text>
      <View style={styles.buttonContainer}>
        <Button
          title="Turn LED ON"
          onPress={() => sendLedCommand('on')}
          color="#4CAF50" // Green
        />
        <View style={styles.spacer} />
        <Button
          title="Turn LED OFF"
          onPress={() => sendLedCommand('off')}
          color="#F44336" // Red
        />
      </View>
      <Text style={styles.hintText}>
        (Ensure your ESP32 is running and connected to Firebase)
      </Text>
      
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#fff',
    alignItems: 'center',
    justifyContent: 'center',
    padding: 20,
  },
  title: {
    fontSize: 24,
    fontWeight: 'bold',
    marginBottom: 20,
  },
  stateText: {
    fontSize: 20,
    marginBottom: 30,
    color: '#333',
  },
  buttonContainer: {
    flexDirection: 'row',
    width: '80%',
    justifyContent: 'space-around',
    marginBottom: 20,
  },
  spacer: {
    width: 20, // Space between buttons
  },
  errorText: {
    color: 'red',
    fontSize: 16,
    textAlign: 'center',
    marginBottom: 10,
  },
  hintText: {
    fontSize: 12,
    color: '#888',
    marginTop: 20,
    textAlign: 'center',
  },
});