# ===============================
# STEP 1: Install Dependencies
# ===============================

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from sklearn.preprocessing import MinMaxScaler
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import SimpleRNN, LSTM, Dense
from tensorflow.keras.optimizers import Adam
from google.colab import files

# ===============================
# STEP 2: Upload Your CSV
# ===============================
print("Please upload your Solar Plant CSV (from Google Sheets/Excel)")
uploaded = files.upload()

# Get filename dynamically
filename = list(uploaded.keys())[0]
df = pd.read_csv(filename)

print("File Loaded Successfully!")
print(df.head())

# ===============================
# STEP 3: Preprocess Data
# ===============================
# Adjust this column name based on your sheet!
data = df[['POWER']].values   # Make sure your sheet has POWER column

# Normalize (0-1)
scaler = MinMaxScaler()
scaled_data = scaler.fit_transform(data)

# Create sequences
def create_sequences(data, seq_length=30):
    X, y = [], []
    for i in range(len(data) - seq_length):
        X.append(data[i:i+seq_length])
        y.append(data[i+seq_length])
    return np.array(X), np.array(y)

SEQ_LEN = 30
X, y = create_sequences(scaled_data, SEQ_LEN)
X = X.reshape((X.shape[0], X.shape[1], 1))

# Train-test split
split = int(0.8 * len(X))
X_train, X_test = X[:split], X[split:]
y_train, y_test = y[:split], y[split:]

# ===============================
# STEP 4: Build & Train RNN Model
# ===============================
rnn_model = Sequential([
    SimpleRNN(64, activation='tanh', input_shape=(SEQ_LEN, 1)),
    Dense(1)
])
rnn_model.compile(optimizer=Adam(0.001), loss='mse')

history_rnn = rnn_model.fit(
    X_train, y_train,
    epochs=20, batch_size=32,
    validation_data=(X_test, y_test),
    verbose=1
)

# ===============================
# STEP 5: Build & Train LSTM Model
# ===============================
lstm_model = Sequential([
    LSTM(64, activation='tanh', input_shape=(SEQ_LEN, 1)),
    Dense(1)
])
lstm_model.compile(optimizer=Adam(0.001), loss='mse')

history_lstm = lstm_model.fit(
    X_train, y_train,
    epochs=20, batch_size=32,
    validation_data=(X_test, y_test),
    verbose=1
)

# ===============================
# STEP 6: Compare Performance
# ===============================
plt.figure(figsize=(10,5))
plt.plot(history_rnn.history['val_loss'], label="RNN Validation Loss", linestyle='dashed')
plt.plot(history_lstm.history['val_loss'], label="LSTM Validation Loss", linestyle='dotted')
plt.title("Validation Loss Comparison: RNN vs LSTM")
plt.xlabel("Epochs")
plt.ylabel("MSE Loss")
plt.legend()
plt.grid(True)
plt.show()

# ===============================
# STEP 7: Predictions
# ===============================
rnn_pred = rnn_model.predict(X_test)
lstm_pred = lstm_model.predict(X_test)

# Rescale back
y_test_rescaled = scaler.inverse_transform(y_test)
rnn_pred_rescaled = scaler.inverse_transform(rnn_pred)
lstm_pred_rescaled = scaler.inverse_transform(lstm_pred)

# Scale to 300W System
# The scaling factor should be based on the maximum value in the original data, not the rescaled data
# Use the scaler's data_range_[0] attribute which represents the difference between the max and min of original data
y_test_scaled = (y_test_rescaled / scaler.data_range_[0]) * 300
rnn_scaled = (rnn_pred_rescaled / scaler.data_range_[0]) * 300
lstm_scaled = (lstm_pred_rescaled / scaler.data_range_[0]) * 300


# ===============================
# STEP 8: Simulated Sensor Values
# ===============================
voltage = 12.0  # Assume constant 12V DC fan load
current = y_test_scaled / voltage

temperature = 25 + 5*np.sin(np.linspace(0,10,len(y_test_scaled)))
humidity = 60 + 10*np.cos(np.linspace(0,10,len(y_test_scaled)))

df_results = pd.DataFrame({
    "Voltage (V)": [voltage]*len(y_test_scaled),
    "Current (A)": current.flatten(),
    "Power (W)": y_test_scaled.flatten(),
    "Temperature (°C)": temperature,
    "Humidity (%)": humidity
})

print("Sample of Real-Time Monitoring Table:")
print(df_results.head(10))

# ===============================
# STEP 9: Final Prediction Graph
# ===============================
plt.figure(figsize=(12,6))
plt.plot(y_test_scaled, label="Actual Solar Output (300W)", color='black')
plt.plot(rnn_scaled, label="RNN Prediction", linestyle='dashed', color='blue')
plt.plot(lstm_scaled, label="LSTM Prediction", linestyle='dotted', color='red')
plt.title("🔆 Solar Power Output Prediction (300W Panel) - RNN vs LSTM")
plt.xlabel("Time Steps")
plt.ylabel("Power Output (Watts)")
plt.ylim(0, 320)
plt.legend()
plt.grid(True)
plt.show()