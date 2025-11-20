# 📊 AWS CDK Dashboard Setup Guide

This directory contains an **AWS CDK application** that automatically provisions a professional CloudWatch dashboard for your ESP8266 temperature logger.

## What It Creates

When deployed, this CDK stack will automatically create:

✅ **CloudWatch Dashboard** with:
- Real-time temperature graph (Celsius & Fahrenheit)
- Device health metrics (uploads, errors)
- Temperature statistics (min, max, average)
- Log Insights queries for recent readings

✅ **CloudWatch Alarms** for:
- High temperature (> 30°C / 86°F)
- Low temperature (< 5°C / 41°F)
- No data received (device offline)

✅ **SNS Topic** for:
- Email notifications when alarms trigger
- Customizable alert thresholds

✅ **Log Groups & Retention**:
- Automatic 30-day retention policy
- Log streaming from your device

---

## Prerequisites

### 1. AWS Account & CLI

```bash
# Install AWS CLI (if not already installed)
curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
unzip awscliv2.zip
sudo ./aws/install

# Configure AWS credentials
aws configure
# Enter: Access Key ID, Secret Access Key, Region (ca-central-1), Output format (json)

# Verify setup
aws sts get-caller-identity
```

### 2. Python 3.7+

```bash
python3 --version
# Should show Python 3.7 or higher
```

### 3. Node.js & npm (for CDK)

```bash
# On Ubuntu/WSL:
sudo apt-get install nodejs npm

# Verify installation
node --version
npm --version
```

### 4. AWS CDK Toolkit

```bash
# Install globally
npm install -g aws-cdk

# Verify installation
cdk --version
```

---

## Setup Steps

### Step 1: Install Python Dependencies

```bash
# From project root directory
pip install -r cdk/requirements.txt
```

### Step 2: Bootstrap CDK (One-time setup)

This prepares your AWS account for CDK deployments:

```bash
cdk bootstrap aws://YOUR_ACCOUNT_ID/ca-central-1
```

Replace `YOUR_ACCOUNT_ID` with your AWS account ID (12 digits):

```bash
# Get your account ID
aws sts get-caller-identity --query Account --output text
```

**Example**:
```bash
cdk bootstrap aws://123456789012/ca-central-1
```

### Step 3: Review the Stack

Before deploying, see what will be created:

```bash
cdk synth
```

This generates the CloudFormation template (stored in `cdk.out/`).

### Step 4: Deploy the Dashboard

```bash
cdk deploy
```

CDK will show what resources will be created. Type **`y`** to confirm.

**Expected output**:
```
✓ TemperatureLoggerDashboardStack

Outputs:
TemperatureLoggerDashboardStack.DashboardURL = https://console.aws.amazon.com/cloudwatch/home?region=ca-central-1#dashboards:name=ESP8266-Temperature-Logger
TemperatureLoggerDashboardStack.AlarmTopicArn = arn:aws:sns:ca-central-1:123456789012:esp8266-temperature-alerts
TemperatureLoggerDashboardStack.LogGroupName = esp-sensor-logs

Stack ARN:
arn:aws:cloudformation:ca-central-1:123456789012:stack/TemperatureLoggerDashboardStack/...
```

---

## Customize the Dashboard

### Change Temperature Thresholds

Edit `cdk/app.py` and modify these lines:

```python
# Alarm: Temperature too high (around line 180)
threshold=30,  # Change from 30°C to desired value

# Alarm: Temperature too low (around line 196)
threshold=5,  # Change from 5°C to desired value
```

Then redeploy:
```bash
cdk deploy
```

### Add Email Notifications

Uncomment these lines in `cdk/app.py` (around line 65):

```python
# Subscribe email (replace with your email)
alarm_topic.add_subscription(
    sns_subs.EmailSubscription("your-email@example.com")
)
```

Then redeploy:
```bash
cdk deploy
```

**Note**: AWS will send a confirmation email. You must **confirm the subscription** for alerts to work.

### Change Log Retention

Edit line ~55 in `cdk/app.py`:

```python
# Set log retention (30 days)
log_group.retention = logs.RetentionDays.ONE_MONTH
```

Options:
- `ONE_WEEK`
- `TWO_WEEKS`
- `ONE_MONTH`
- `THREE_MONTHS`
- `SIX_MONTHS`
- `ONE_YEAR`
- `THIRTEEN_MONTHS`
- `FOREVER`

---

## View Your Dashboard

### Option 1: Use CDK Output

After deployment, CDK prints the dashboard URL:

```
DashboardURL = https://console.aws.amazon.com/cloudwatch/home?region=ca-central-1#dashboards:name=ESP8266-Temperature-Logger
```

Click the link or copy/paste into browser.

### Option 2: AWS Console

1. **AWS Console** → **CloudWatch**
2. **Dashboards** (left sidebar)
3. Click **ESP8266-Temperature-Logger**

---

## What the Dashboard Shows

### 1. Temperature Over Time (Top Left)
- **Left Y-axis**: Celsius (°C)
- **Right Y-axis**: Fahrenheit (°F)
- **Time range**: Last 24 hours by default (customizable)
- **Statistics**: Average temperature every 1 minute

### 2. Current Temperature (Top Right)
- **Single value widget**
- Shows average temperature for last 5 minutes
- Easy reference without looking at graph

### 3. Device Health (Middle)
- **Successful uploads**: Count of successful Lambda uploads
- **Error count**: Number of errors in device logs

### 4. 24-Hour Statistics (Bottom Left)
- **Max temperature**: Highest reading in last 24 hours
- **Min temperature**: Lowest reading in last 24 hours
- **Avg temperature**: Average of all readings

### 5. Recent Readings (Bottom Right)
- **CloudWatch Logs Insights query**
- Shows detailed temperature readings from logs
- Updated in real-time as device sends data

---

## Manage Alarms

### View Alarm Status

```bash
aws cloudwatch describe-alarms --alarm-names "ESP8266-HighTemperature"
```

### Test Alarm

Trigger high temperature alarm:
```bash
aws cloudwatch set-alarm-state \
  --alarm-name ESP8266-HighTemperature \
  --state-value ALARM \
  --state-reason "Testing alarm"
```

### Disable Alarm

```bash
aws cloudwatch disable-alarm-actions \
  --alarm-names "ESP8266-HighTemperature"
```

---

## Update Dashboard

Make changes to `cdk/app.py` and redeploy:

```bash
# Review changes
cdk diff

# Deploy updates
cdk deploy
```

---

## Destroy Resources

To remove all dashboard, alarms, and SNS topic:

```bash
cdk destroy
```

**Warning**: This removes the dashboard and alarms but **keeps the log group** intact (for safety).

To also delete the log group:
```bash
aws logs delete-log-group --log-group-name esp-sensor-logs
```

---

## Troubleshooting

### CDK Command Not Found

```bash
# Install globally
npm install -g aws-cdk

# Or use npx
npx cdk --version
```

### AWS Credentials Error

```bash
# Configure credentials
aws configure

# Verify
aws sts get-caller-identity
```

### Python Module Not Found

```bash
# Install requirements
pip install -r cdk/requirements.txt

# Or use venv
python3 -m venv venv
source venv/bin/activate
pip install -r cdk/requirements.txt
```

### Bootstrap Error

```bash
# Get your account ID
ACCOUNT_ID=$(aws sts get-caller-identity --query Account --output text)

# Bootstrap (one-time)
cdk bootstrap aws://$ACCOUNT_ID/ca-central-1
```

### IAM Permission Denied

Ensure your AWS user has permissions for:
- `cloudwatch:*`
- `logs:*`
- `sns:*`
- `cloudformation:*`

Contact your AWS administrator if needed.

---

## Files Structure

```
cdk/
├── app.py                 # Main CDK stack definition
├── requirements.txt       # Python dependencies
└── README.md             # This file

cdk.out/                  # Generated CloudFormation template (auto-created)
└── TemperatureLoggerDashboardStack.json
```

---

## Next Steps

1. ✅ Deploy dashboard: `cdk deploy`
2. ✅ Add email notifications (uncomment in `app.py`)
3. ✅ Customize thresholds (edit `app.py`)
4. ✅ Monitor in CloudWatch Dashboards
5. ✅ Set up Grafana for additional visualizations (optional)

---

## Example Deployment Session

```bash
# Clone/navigate to project
cd esp12f_ds18b20_temp_sensor

# Install dependencies
pip install -r cdk/requirements.txt

# Configure AWS
aws configure
# Enter: Access Key, Secret, Region (ca-central-1)

# Get account ID for bootstrap
ACCOUNT_ID=$(aws sts get-caller-identity --query Account --output text)

# Bootstrap (one-time)
cdk bootstrap aws://$ACCOUNT_ID/ca-central-1

# Synthesize template
cdk synth

# Deploy dashboard
cdk deploy

# View output
# Copy dashboard URL and open in browser

# Make changes
# Edit cdk/app.py to customize

# Update deployment
cdk deploy

# When done, destroy (optional)
cdk destroy
```

---

## Support

- **AWS CDK Docs**: https://docs.aws.amazon.com/cdk/latest/guide/
- **CloudWatch Docs**: https://docs.aws.amazon.com/cloudwatch/
- **Python CDK Examples**: https://github.com/aws-samples/aws-cdk-examples

---

**Happy Monitoring!** 📊🌡️
