import streamlit as st
import requests
import json
import time
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import pandas as pd
from datetime import datetime, timedelta
import io
from fpdf import FPDF
import google.ai.generativelanguage as glm
from google.generativeai import configure, GenerativeModel
import base64
import os

# Configure page
st.set_page_config(
    page_title="HealthX - ESP32 Health Monitor",
    page_icon="🏥",
    layout="wide",
    initial_sidebar_state="expanded"
)

# ESP32 IP configuration
ESP32_IP = "172.20.10.2"  # Replace with your ESP32 IP
ESP32_URL = f"http://{ESP32_IP}/data"

# Gemini API key (replace with your actual key or use environment variable)
GEMINI_API_KEY = os.getenv("GEMINI_API_KEY", "AIzaSyD6Mvxuz2quV2r7MdHrT15B9DlAIA0INbE")
configure(api_key=GEMINI_API_KEY)

# Authentication credentials
VALID_CREDENTIALS = {
    "username": "bhuvaneshb",
    "password": "ABCD1234",
    "patient_id": "CD001"
}

# Initialize session state
if 'authenticated' not in st.session_state:
    st.session_state.authenticated = False
if 'sensor_data' not in st.session_state:
    st.session_state.sensor_data = []
if 'chat_history' not in st.session_state:
    st.session_state.chat_history = []

def authenticate_user(username, password, patient_id):
    """Authenticate user credentials"""
    return (username == VALID_CREDENTIALS["username"] and 
            password == VALID_CREDENTIALS["password"] and 
            patient_id == VALID_CREDENTIALS["patient_id"])

def login_page():
    """Display login page"""
    st.markdown("""
    <div style='text-align: center; padding: 2rem;'>
        <h1 style='color: #2E86AB; font-size: 3rem;'>🏥 HealthX - ESP32 Health Monitor</h1>
        <p style='font-size: 1.2rem; color: #666;'>Secure Patient Data Access</p>
    </div>
    """, unsafe_allow_html=True)
    
    col1, col2, col3 = st.columns([1, 2, 1])
    
    with col2:
        with st.container():
            st.markdown("""
            <div style='background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); 
                        padding: 2rem; border-radius: 15px; box-shadow: 0 10px 30px rgba(0,0,0,0.1);'>
            """, unsafe_allow_html=True)
            
            st.markdown("<h3 style='color: white; text-align: center; margin-bottom: 1.5rem;'>Patient Login</h3>", unsafe_allow_html=True)
            
            username = st.text_input("👤 Username", placeholder="Enter your username")
            password = st.text_input("🔒 Password", type="password", placeholder="Enter your password")
            patient_id = st.text_input("🆔 Patient ID", placeholder="Enter patient ID")
            
            col_btn1, col_btn2, col_btn3 = st.columns([1, 2, 1])
            with col_btn2:
                if st.button("🚀 Login", use_container_width=True):
                    if authenticate_user(username, password, patient_id):
                        st.session_state.authenticated = True
                        st.success("✅ Login successful! Redirecting...")
                        time.sleep(1)
                        st.rerun()
                    else:
                        st.error("❌ Invalid credentials. Please try again.")
            
            st.markdown("</div>", unsafe_allow_html=True)

def fetch_sensor_data():
    """Fetch data from ESP32"""
    try:
        response = requests.get(ESP32_URL, timeout=5)
        if response.status_code == 200:
            data = response.json()
            data['timestamp'] = datetime.now()
            return data
        else:
            return None
    except requests.RequestException:
        return None

def create_gauge_chart(value, title, min_val, max_val, color, unit="", optimal_range=None):
    """Create a gauge chart for sensor data"""
    fig = go.Figure(go.Indicator(
        mode = "gauge+number+delta",
        value = value,
        domain = {'x': [0, 1], 'y': [0, 1]},
        title = {'text': f"{title} ({unit})", 'font': {'color': 'white'}},
        number = {'font': {'color': 'white'}},
        delta = {
            'reference': (min_val + max_val) / 2,
            'increasing': {'color': 'lightgreen'},
            'decreasing': {'color': 'red'},
            'font': {'color': 'white'}
        },
        gauge = {
            'axis': {'range': [None, max_val]},
            'bar': {'color': color},
            'steps': [
                {'range': [min_val, max_val * 0.5], 'color': "lightgray"},
                {'range': [max_val * 0.5, max_val * 0.8], 'color': "gray"}
            ],
            'threshold': {
                'line': {'color': "red", 'width': 4},
                'thickness': 0.75,
                'value': max_val * 0.9
            }
        }
    ))
    
    if optimal_range:
        fig.add_shape(
            type="rect",
            x0=0, y0=optimal_range[0]/max_val,
            x1=1, y1=optimal_range[1]/max_val,
            fillcolor="green",
            opacity=0.2,
            layer="below",
            line_width=0,
        )
    
    fig.update_layout(
        height=300,
        font={'color': "white", 'family': "Arial"},
        paper_bgcolor="rgba(0,0,0,0)",
        plot_bgcolor="rgba(0,0,0,0)"
    )
    
    return fig

def get_health_status(data):
    """Determine overall health status"""
    alerts = []
    
    # Temperature check
    if data.get('temperature', 0) < 18 or data.get('temperature', 0) > 28:
        alerts.append("Room temperature outside optimal range")
    
    # Humidity check
    if data.get('humidity', 0) < 40 or data.get('humidity', 0) > 60:
        alerts.append("Room humidity outside optimal range")
    
    # Heart rate check
    if data.get('heart_rate', 0) < 60 or data.get('heart_rate', 0) > 100:
        alerts.append("Heart rate outside normal range")
    
    # SpO2 check
    if data.get('spo2', 0) < 95:
        alerts.append("Blood oxygen level low")
    
    # Body temperature check
    if data.get('body_temperature', 0) < 36.1 or data.get('body_temperature', 0) > 37.2:
        alerts.append("Body temperature abnormal")
    
    return alerts

def generate_health_report(data_history):
    """Generate PDF health report"""
    pdf = FPDF()
    pdf.add_page()
    pdf.set_font("Arial", size=16)
    
    # Title
    pdf.cell(200, 10, "HealthX - ESP32 Health Monitor Report", ln=1, align='C')
    pdf.cell(200, 10, f"Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}", ln=1, align='C')
    pdf.ln(10)
    
    # Patient Info
    pdf.set_font("Arial", size=12)
    pdf.cell(200, 10, "Patient Information:", ln=1)
    pdf.cell(200, 10, f"Patient ID: {VALID_CREDENTIALS['patient_id']}", ln=1)
    pdf.cell(200, 10, f"Username: {VALID_CREDENTIALS['username']}", ln=1)
    pdf.ln(10)
    
    # Recent readings
    if data_history:
        latest_data = data_history[-1]
        pdf.cell(200, 10, "Latest Readings:", ln=1)
        pdf.cell(200, 10, f"Room Temperature: {latest_data.get('temperature', 'N/A')} °C", ln=1)
        pdf.cell(200, 10, f"Room Humidity: {latest_data.get('humidity', 'N/A')} %", ln=1)
        pdf.cell(200, 10, f"Heart Rate: {latest_data.get('heart_rate', 'N/A')} BPM", ln=1)
        pdf.cell(200, 10, f"Blood Oxygen: {latest_data.get('spo2', 'N/A')} %", ln=1)
        pdf.cell(200, 10, f"Body Temperature: {latest_data.get('body_temperature', 'N/A')} °C", ln=1)
        
        # Health alerts
        alerts = get_health_status(latest_data)
        if alerts:
            pdf.ln(10)
            pdf.cell(200, 10, "Health Alerts:", ln=1)
            for alert in alerts:
                pdf.cell(200, 10, f"- {alert}", ln=1)
    
    return pdf.output(dest='S').encode('latin1')

def chat_with_ai(user_message, sensor_data):
    """Chat with Google Gemini API about health data"""
    try:
        # Prepare context with sensor data
        context = f"""
        You are HealthX, an advanced health monitoring AI assistant for the HealthX platform. The patient's current sensor readings are:
        - Room Temperature: {sensor_data.get('temperature', 'N/A')} °C
        - Room Humidity: {sensor_data.get('humidity', 'N/A')} %
        - Heart Rate: {sensor_data.get('heart_rate', 'N/A')} BPM
        - Blood Oxygen (SpO2): {sensor_data.get('spo2', 'N/A')} %
        - Body Temperature: {sensor_data.get('body_temperature', 'N/A')} °C
        
        Provide personalized, accurate, and concise health advice based on these readings and the user's question. Highlight any concerning values and suggest actionable next steps. Keep responses professional, empathetic, and under 500 tokens.
        """
        
        # Initialize Gemini model
        model = GenerativeModel(
            model_name="models/gemini-2.0-flash",
            generation_config={
                "max_output_tokens": 500,
                "temperature": 0.1
            }
        )
        
        # Combine context and user message
        prompt = f"{context}\n\nUser Question: {user_message}"
        
        # Generate response
        response = model.generate_content(prompt)
        
        return response.text
    except Exception as e:
        return f"Sorry, I'm having trouble connecting to the AI service. Error: {str(e)}"

def main_dashboard():
    """Main dashboard page"""
    st.markdown("""
    <style>
        .section-container {
            border: 1px solid #e0e0e0;
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 20px;
            background-color: rgba(255, 255, 255, 0.05);
        }
    </style>
    <div style='text-align: center; padding: 1rem; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); 
                border-radius: 10px; margin-bottom: 2rem;'>
        <h1 style='color: white; margin: 0;'>🏥 HealthX - ESP32 Health Monitor Dashboard</h1>
        <p style='color: white; margin: 0;'>Real-time Patient Monitoring System</p>
    </div>
    """, unsafe_allow_html=True)
    
    # Auto-refresh data
    placeholder = st.empty()
    
    # Sidebar controls
    st.sidebar.header("🎛️ Controls")
    auto_refresh = st.sidebar.checkbox("Auto Refresh", value=True)
    refresh_interval = st.sidebar.slider("Refresh Interval (seconds)", 1, 10, 2)
    
    if st.sidebar.button("🔄 Manual Refresh"):
        st.rerun()
    
    if st.sidebar.button("📊 Generate Report"):
        if st.session_state.sensor_data:
            pdf_data = generate_health_report(st.session_state.sensor_data)
            pdf_bytes = pdf_data  # Already in bytes
            st.sidebar.download_button(
                label="📥 Download PDF Report",
                data=pdf_bytes,
                file_name=f"health_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.pdf",
                mime="application/pdf"
            )
        else:
            st.sidebar.warning("No data available for report generation")
    
    if st.sidebar.button("🚪 Logout"):
        st.session_state.authenticated = False
        st.session_state.sensor_data = []
        st.session_state.chat_history = []
        st.rerun()
    
    # Fetch and display data
    with placeholder.container():
        data = fetch_sensor_data()
        
        if data:
            # Store data in session state
            st.session_state.sensor_data.append(data)
            # Keep only last 100 readings
            if len(st.session_state.sensor_data) > 100:
                st.session_state.sensor_data = st.session_state.sensor_data[-100:]
            
            # Alerts Section
            with st.container():
                st.markdown('<div class="section-container">', unsafe_allow_html=True)
                st.subheader("🚨 Health Alerts")
                col_status1, col_status2 = st.columns([3, 1])
                with col_status1:
                    st.success("🟢 Connected to ESP32 - Data Updated")
                with col_status2:
                    st.metric("📡 Last Update", datetime.now().strftime("%H:%M:%S"))
                alerts = get_health_status(data)
                if alerts:
                    st.warning("⚠️ Health Alerts:")
                    for alert in alerts:
                        st.write(f"• {alert}")
                else:
                    st.info("✅ No health alerts at this time.")
                st.markdown('</div>', unsafe_allow_html=True)
            
            # Real-Time Meters Section
            with st.container():
                st.markdown('<div class="section-container">', unsafe_allow_html=True)
                st.subheader("📏 Real-Time Health Metrics")
                col1, col2, col3 = st.columns(3)
                
                with col1:
                    temp_gauge = create_gauge_chart(
                        data.get('temperature', 0), 
                        "Room Temperature", 
                        0, 50, 
                        "blue", 
                        "°C", 
                        optimal_range=[20, 25]
                    )
                    st.plotly_chart(temp_gauge, use_container_width=True)
                    
                    humidity_gauge = create_gauge_chart(
                        data.get('humidity', 0), 
                        "Room Humidity", 
                        0, 100, 
                        "lightblue", 
                        "%", 
                        optimal_range=[40, 60]
                    )
                    st.plotly_chart(humidity_gauge, use_container_width=True)
                
                with col2:
                    hr_gauge = create_gauge_chart(
                        data.get('heart_rate', 0), 
                        "Heart Rate", 
                        0, 150, 
                        "red", 
                        "BPM", 
                        optimal_range=[60, 100]
                    )
                    st.plotly_chart(hr_gauge, use_container_width=True)
                    
                    spo2_gauge = create_gauge_chart(
                        data.get('spo2', 0), 
                        "Blood Oxygen", 
                        0, 100, 
                        "green", 
                        "%", 
                        optimal_range=[95, 100]
                    )
                    st.plotly_chart(spo2_gauge, use_container_width=True)
                
                with col3:
                    body_temp_gauge = create_gauge_chart(
                        data.get('body_temperature', 0), 
                        "Body Temperature", 
                        30, 45, 
                        "orange", 
                        "°C", 
                        optimal_range=[36.1, 37.2]
                    )
                    st.plotly_chart(body_temp_gauge, use_container_width=True)
                st.markdown('</div>', unsafe_allow_html=True)
            
            # Historical Graphs Section
            with st.container():
                st.markdown('<div class="section-container">', unsafe_allow_html=True)
                st.subheader("📈 Historical Data")
                if len(st.session_state.sensor_data) > 1:
                    df = pd.DataFrame(st.session_state.sensor_data)
                    df['timestamp'] = pd.to_datetime(df['timestamp'])
                    
                    fig = make_subplots(
                        rows=3, cols=2,
                        subplot_titles=('Room Temperature', 'Room Humidity', 'Heart Rate', 'Blood Oxygen', 'Body Temperature'),
                        specs=[[{"secondary_y": False}, {"secondary_y": False}],
                               [{"secondary_y": False}, {"secondary_y": False}],
                               [{"secondary_y": False}, {"type": "xy"}]]
                    )
                    
                    fig.add_trace(go.Scatter(x=df['timestamp'], y=df['temperature'], name='Room Temp'), row=1, col=1)
                    fig.add_trace(go.Scatter(x=df['timestamp'], y=df['humidity'], name='Humidity'), row=1, col=2)
                    fig.add_trace(go.Scatter(x=df['timestamp'], y=df['heart_rate'], name='Heart Rate'), row=2, col=1)
                    fig.add_trace(go.Scatter(x=df['timestamp'], y=df['spo2'], name='SpO2'), row=2, col=2)
                    fig.add_trace(go.Scatter(x=df['timestamp'], y=df['body_temperature'], name='Body Temp'), row=3, col=1)
                    
                    fig.update_layout(height=800, showlegend=False)
                    st.plotly_chart(fig, use_container_width=True)
                else:
                    st.info("Not enough data for historical graphs.")
                st.markdown('</div>', unsafe_allow_html=True)
            
            # AI Chatbot Section
            with st.container():
                st.markdown('<div class="section-container">', unsafe_allow_html=True)
                st.subheader("🤖 HealthX AI Health Assistant")
                for i, (question, answer) in enumerate(st.session_state.chat_history):
                    with st.expander(f"Q{i+1}: {question[:50]}..." if len(question) > 50 else f"Q{i+1}: {question}"):
                        st.write(f"**Question:** {question}")
                        st.write(f"**Answer:** {answer}")
                
                user_question = st.text_input("Ask HealthX about your health data:", placeholder="e.g., Is my heart rate normal?")
                
                if st.button("💬 Ask AI") and user_question:
                    with st.spinner("HealthX is analyzing your data..."):
                        ai_response = chat_with_ai(user_question, data)
                        st.session_state.chat_history.append((user_question, ai_response))
                        st.rerun()
                st.markdown('</div>', unsafe_allow_html=True)
        
        else:
            st.error("❌ Unable to connect to ESP32. Please check the connection.")
            st.info(f"Trying to connect to: {ESP32_URL}")
    
    # Auto refresh
    if auto_refresh:
        time.sleep(refresh_interval)
        st.rerun()

# Main app logic
def main():
    if not st.session_state.authenticated:
        login_page()
    else:
        main_dashboard()

if __name__ == "__main__":
    main()