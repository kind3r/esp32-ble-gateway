<template>
  <div id="app">
    <div class="container">
      <div class="container text-center">
        <h1>BLE to WiFi Gateway configuration</h1>
        <div class="spinner-border" role="status" v-if="!ready">
          <span class="visually-hidden">Loading...</span>
        </div>
      </div>
      <div class="row" v-if="ready">
        <form>
          <div class="mb-3">
            <label for="name" class="form-label">Gateway name</label>
            <input type="text" class="form-control" id="name" aria-describedby="nameHelp" v-model="name" />
            <div id="nameHelp" class="form-text">
              Your gateway will have the local MDNS name: <code v-if="name.length > 0">{{ name }}.local</code>
            </div>
          </div>
          <div class="mb-3">
            <label for="ssid" class="form-label">WiFi SSID</label>
            <input type="text" class="form-control" id="ssid" aria-describedby="ssidHelp" v-model="wifi_ssid" />
            <div id="ssidHelp" class="form-text">Name of your wireless network.</div>
          </div>
          <div class="mb-3">
            <label for="pass" class="form-label">WiFi Password</label>
            <div class="input-group">
              <input :type="attrWifiPassType" class="form-control" id="pass" v-model="wifi_pass" />
              <button class="btn btn-primary" type="button" v-on:click="toggleShowWifiPass">Show</button>
            </div>
          </div>
          <div class="mb-3">
            <label for="aes" class="form-label">AES Key</label>
            <div class="input-group">
              <input class="form-control" :type="attrAesType" id="aes" v-model="aes_key" />
              <button class="btn btn-primary" type="button" v-on:click="toggleShowAes">Show</button>
            </div>
          </div>

          <button type="submit" class="btn btn-primary disabled">Save configuration</button>
        </form>
      </div>
    </div>
    <div class="toast-container position-absolute p-3 bottom-0 start-50 translate-middle-x" id="toastPlacement">
      <div class="toast d-flex text-white bg-danger align-items-center" :class="{ show: errors.length > 0 }">
        <div class="toast-body">
          <ul>
            <li v-for="error in errors" :key="error">
              {{ error }}
            </li>
          </ul>
        </div>
        <button type="button" class="btn-close btn-close-white ms-auto me-2" v-on:click="clearErrors"></button>
      </div>
    </div>
  </div>
</template>

<script>
import axios from "axios";
import "./assets/bootstrap.min.css";

export default {
  name: "App",
  data: function () {
    return {
      name: "",
      wifi_ssid: "",
      wifi_pass: "",
      aes_key: "ssss",
      ready: false,
      errors: [],
      attrWifiPassType: "password",
      attrAesType: "password",
    };
  },
  computed: {
    hasErrors() {
      return this.errors.length > 0;
    },
  },
  methods: {
    async loadConfig() {
      try {
        const response = await axios.get("/config");
        const config = response.data;
        this.name = config.name;
        this.wifi_ssid = config.wifi_ssid;
        this.wifi_pass = config.wifi_pass;
        this.aes_key = config.aes_key;
        this.ready = true;
        return true;
      } catch (error) {
        if (typeof error.toString != "undefined") {
          this.errors.push(error.toString());
        } else {
          console.log(error);
          this.errors.push("Error fetch configuration data");
        }
      }
      return false;
    },
    toggleShowWifiPass() {
      if (this.attrWifiPassType == "password") {
        this.attrWifiPassType = "text";
      } else {
        this.attrWifiPassType = "password";
      }
    },
    toggleShowAes() {
      if (this.attrAesType == "password") {
        this.attrAesType = "text";
      } else {
        this.attrAesType = "password";
      }
    },
    clearErrors() {
      this.errors = [];
    },
  },
  async created() {
    await this.loadConfig();
  },
};
</script>
