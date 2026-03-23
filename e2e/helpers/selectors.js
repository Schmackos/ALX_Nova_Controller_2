/**
 * Reusable DOM selectors for ALX Nova Controller 2 WebGUI E2E tests.
 * All selectors are based on actual IDs and classes in web_src/index.html.
 */

const SELECTORS = {
  // ===== Sidebar navigation =====
  sidebar: '#sidebar',
  sidebarItems: '.sidebar-item',
  sidebarTab: (tabName) => `.sidebar-item[data-tab="${tabName}"]`,
  sidebarVersion: '#sidebarVersion',
  sidebarToggle: '.sidebar-toggle',

  // ===== Mobile bottom tab bar =====
  tabBar: '.tab-bar',
  tabBarItems: '.tab-bar .tab',
  tabBarTab: (tabName) => `.tab-bar .tab[data-tab="${tabName}"]`,

  // ===== Status bar =====
  statusBar: '.status-bar',
  statusAmpIndicator: '#statusAmp',
  statusAmpText: '#statusAmpText',
  statusWifiIndicator: '#statusWifi',
  statusWifiText: '#statusWifiText',
  statusMqttIndicator: '#statusMqtt',
  statusMqttText: '#statusMqttText',
  statusWsIndicator: '#statusWs',

  // ===== Tab panels =====
  panel: (tabName) => `#${tabName}`,
  activePanel: '.panel.active',

  // ===== Control tab =====
  wsConnectionStatus: '#wsConnectionStatus',
  signalDetected: '#signalDetected',
  audioLevel: '#audioLevel',
  audioVrms: '#audioVrms',
  infoSensingMode: '#infoSensingMode',
  infoTimerDuration: '#infoTimerDuration',
  infoAudioThreshold: '#infoAudioThreshold',
  amplifierStatus: '#amplifierStatus',
  amplifierDisplay: '#amplifierDisplay',
  timerDisplay: '#timerDisplay',
  timerValue: '#timerValue',
  sensingModeRadios: 'input[name="sensingMode"]',
  sensingModeRadio: (value) => `input[name="sensingMode"][value="${value}"]`,
  smartAutoSettingsCard: '#smartAutoSettingsCard',
  timerDurationInput: '#appState\\.timerDuration',
  audioThresholdInput: '#audioThreshold',
  manualOnBtn: 'button.btn-success',
  manualOffBtn: 'button.btn-danger',

  // ===== Audio tab =====
  audioPanel: '#audio',
  audioSubNav: '.audio-subnav',
  audioSubNavBtn: (view) => `.audio-subnav-btn[data-view="${view}"]`,
  audioSubView: (view) => `#audio-sv-${view}`,
  audioInputsContainer: '#audio-inputs-container',
  audioMatrixContainer: '#audio-matrix-container',
  audioOutputsContainer: '#audio-outputs-container',

  // Waveform / spectrum / VU
  waveformEnabledToggle: '#waveformEnabledToggle',
  spectrumEnabledToggle: '#spectrumEnabledToggle',
  vuMeterEnabledToggle: '#vuMeterEnabledToggle',
  waveformCanvas0: '#audioWaveformCanvas0',
  waveformCanvas1: '#audioWaveformCanvas1',
  spectrumCanvas0: '#audioSpectrumCanvas0',
  spectrumCanvas1: '#audioSpectrumCanvas1',

  // Signal generator
  siggenEnable: '#siggenEnable',
  siggenFields: '#siggenFields',
  siggenWaveform: '#siggenWaveform',
  siggenFreq: '#siggenFreq',
  siggenFreqVal: '#siggenFreqVal',
  siggenAmp: '#siggenAmp',
  siggenAmpVal: '#siggenAmpVal',
  siggenOutputMode: '#siggenOutputMode',
  siggenTargetAdc: '#siggenTargetAdc',

  // ===== Devices (HAL) tab =====
  devicesPanel: '#devices',
  halDeviceList: '#hal-device-list',
  halRescanBtn: '#hal-rescan-btn',
  halDeviceCards: '.hal-device-card',
  halDeviceHeader: '.hal-device-header',

  // ===== Ethernet (within Network/WiFi tab) =====
  networkOverviewCard: '#networkOverviewCard',
  activeInterfaceText: '#activeInterfaceText',
  networkHostnameDisplay: '#networkHostnameDisplay',
  apAccessBanner: '#apAccessBanner',
  ethStatusCard: '#ethStatusCard',
  ethStatusBox: '#ethStatusBox',
  ethLinkStatusText: '#ethLinkStatusText',
  ethMacAddress: '#ethMacAddress',
  ethConfigCard: '#ethConfigCard',
  ethHostnameInput: '#ethHostnameInput',
  ethUseStaticIP: '#ethUseStaticIP',
  ethStaticIPFields: '#ethStaticIPFields',
  ethStaticIP: '#ethStaticIP',
  ethSubnetInput: '#ethSubnetInput',
  ethGatewayInput: '#ethGatewayInput',
  ethDns1Input: '#ethDns1Input',
  ethDns2Input: '#ethDns2Input',
  ethConfirmModal: '#ethConfirmModal',
  ethConfirmCountdown: '#ethConfirmCountdown',

  // ===== WiFi tab =====
  wifiPanel: '#wifi',
  wifiStatusBox: '#wifiStatusBox',
  wifiNetworkSelect: '#wifiNetworkSelect',
  scanBtn: '#scanBtn',
  wifiSsidInput: '#appState\\.wifiSSID',
  wifiPasswordInput: '#appState\\.wifiPassword',
  useStaticIPToggle: '#useStaticIP',
  staticIPFields: '#staticIPFields',
  configNetworkSelect: '#configNetworkSelect',
  apToggle: '#apToggle',

  // ===== MQTT tab =====
  mqttPanel: '#mqtt',
  mqttStatusBox: '#mqttStatusBox',
  mqttEnabledToggle: '#appState\\.mqttEnabled',
  mqttFields: '#mqttFields',
  mqttBrokerInput: '#appState\\.mqttBroker',
  mqttPortInput: '#appState\\.mqttPort',
  mqttUsernameInput: '#appState\\.mqttUsername',
  mqttBaseTopicInput: '#appState\\.mqttBaseTopic',
  mqttHADiscovery: '#appState\\.mqttHADiscovery',

  // ===== Settings tab =====
  settingsPanel: '#settings',
  darkModeToggle: '#darkModeToggle',
  debugModeToggle: '#debugModeToggle',
  buzzerToggle: '#buzzerToggle',
  buzzerFields: '#buzzerFields',
  buzzerVolumeSelect: '#buzzerVolumeSelect',
  backlightToggle: '#backlightToggle',
  brightnessSelect: '#brightnessSelect',
  screenTimeoutSelect: '#screenTimeoutSelect',
  dimToggle: '#dimToggle',
  currentVersion: '#currentVersion',
  latestVersion: '#latestVersion',
  checkForUpdatesBtn: 'button[onclick="checkForUpdate()"]',
  changePasswordBtn: 'button[onclick="showPasswordChangeModal()"]',

  // ===== Debug tab =====
  debugPanel: '#debug',
  debugConsole: '#debugConsole',
  logLevelFilter: '#logLevelFilter',
  moduleChips: '#moduleChips',
  debugSearchInput: '#debugSearchInput',
  pauseBtn: '#pauseBtn',
  timestampToggle: '#timestampToggle',
  debugHwStatsToggle: '#debugHwStatsToggle',
  debugI2sMetricsToggle: '#debugI2sMetricsToggle',
  debugTaskMonitorToggle: '#debugTaskMonitorToggle',
  debugSerialLevel: '#debugSerialLevel',

  // Hardware stats elements referenced by debug/stats
  cpuTotal: '#cpuTotal',
  cpuTemp: '#cpuTemp',
  heapPercent: '#heapPercent',
  heapFree: '#heapFree',
  psramPercent: '#psramPercent',
  uptime: '#uptime',

  // ===== Support tab =====
  supportPanel: '#support',
  manualRendered: '#manualRendered',
  manualLink: '#manualLink',
  manualQrCode: '#manualQrCode',
  manualSearchInput: '#manualSearchInput',

  // ===== Auth / Password change modal =====
  passwordChangeModal: '#passwordChangeModal',
  currentPasswordInput: '#currentPassword',
  currentPasswordGroup: '#currentPasswordGroup',
  newPasswordInput: '#newPassword',
  confirmPasswordInput: '#confirmPassword',
  passwordError: '#passwordError',
};

module.exports = SELECTORS;
