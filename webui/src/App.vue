<template>
  <div id="app">
    <div class="container">
      <div class="container text-center">
        <h1>Gateway configuration</h1>
        <div class="spinner-border" role="status" v-if="!ready">
          <span class="visually-hidden">Loading...</span>
        </div>
      </div>
      <form>
        <div class="row" v-if="ready">
          <div class="col col-6 mb-3">
            <label for="name" class="form-label">Gateway name</label>
            <input type="text" class="form-control" id="name" aria-describedby="nameHelp" v-model="name" />
            <div id="nameHelp" class="form-text">
              MDNS name: <code v-if="name.length > 0">{{ name }}.local</code>
            </div>
          </div>
          <div class="col col-6 mb-3">
            <label for="password" class="form-label">Gateway password</label>
            <div class="input-group">
              <input :type="attrPasswordType" class="form-control" id="password" aria-describedby="passwordHelp" v-model="password" />
              <button class="btn btn-primary" type="button" v-on:click="toggleShowPassword">
                <img class="icon" src="@/assets/fa/eye.svg?data" v-if="attrPasswordType == 'password'" />
                <img class="icon" src="@/assets/fa/eye-slash.svg?data" v-else />
              </button>
            </div>
            <div id="passwordHelp" class="form-text">Username is always <code>admin</code>. Empty = no change</div>
          </div>
          <div class="col col-6 mb-3">
            <label for="ssid" class="form-label">WiFi SSID</label>
            <input type="text" class="form-control" id="ssid" v-model="wifi_ssid" />
          </div>
          <div class="col col-6 mb-3">
            <label for="pass" class="form-label">WiFi Password</label>
            <div class="input-group">
              <input :type="attrWifiPassType" class="form-control" id="pass" v-model="wifi_pass" />
              <button class="btn btn-primary" type="button" v-on:click="toggleShowWifiPass">
                <img class="icon" src="@/assets/fa/eye.svg?data" v-if="attrWifiPassType == 'password'" />
                <img class="icon" src="@/assets/fa/eye-slash.svg?data" v-else />
              </button>
            </div>
          </div>
          <div class="col col-12 mb-3">
            <label for="aes" class="form-label">AES Key</label>
            <div class="input-group">
              <input class="form-control" :type="attrAesType" id="aes" v-model="aes_key" />
              <button class="btn btn-primary" type="button" v-on:click="toggleShowAes">
                <img class="icon" src="@/assets/fa/eye.svg?data" v-if="attrAesType == 'password'" />
                <img class="icon" src="@/assets/fa/eye-slash.svg?data" v-else />
              </button>
            </div>
          </div>

          <div class="col col12 text-center mt-3">
            <a href="javascript:void(0)" class="btn btn-primary" :class="{ disabled: !configChanged || saving }" v-on:click="saveConfig">
              <div class="spinner-border spinner-border-sm" role="status" v-if="saving">
                <span class="visually-hidden">Loading...</span>
              </div>
              Save configuration
            </a>
          </div>
        </div>
      </form>
    </div>
    <div class="toast-container position-absolute p-3 bottom-0 start-50 translate-middle-x" v-if="errors.length > 0 || savingSuccess">
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
      <div class="toast d-flex text-white bg-success align-items-center" :class="{ show: savingSuccess }">
        <div class="toast-body">Configuration saved. ESP now rebooting.</div>
        <button type="button" class="btn-close btn-close-white ms-auto me-2" v-on:click="savingSuccess = false"></button>
      </div>
    </div>
  </div>
</template>

<script>
import axios from "axios";
import "./assets/bootstrap.min.css";

export default {
  name: "App",
  components: {},
  data: function () {
    return {
      config: {},
      name: "",
      password: "",
      wifi_ssid: "",
      wifi_pass: "",
      aes_key: "ssss",
      ready: false,
      errors: [],
      attrPasswordType: "password",
      attrWifiPassType: "password",
      attrAesType: "password",
      saving: false,
      savingSuccess: false,
    };
  },
  computed: {
    hasErrors() {
      return this.errors.length > 0;
    },
    configChanged() {
      const config = this.config;
      if (config.name != this.name) return true;
      if (this.password != "") return true;
      if (config.wifi_ssid != this.wifi_ssid) return true;
      if (config.wifi_pass != this.wifi_pass) return true;
      if (config.aes_key != this.aes_key) return true;
      return false;
    },
  },
  methods: {
    async loadConfig() {
      try {
        const response = await axios.get("/config", {
          withCredentials: true,
        });
        const config = response.data;
        this.config = config;
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
          this.errors.push("Error fetching configuration data");
        }
      }
      return false;
    },
    async saveConfig() {
      if (this.configChanged && !this.saving) {
        this.saving = true;
        this.errors = [];
        // clone the current configuration so we can replace it later
        let config = JSON.parse(JSON.stringify(this.config));
        // only send changed values
        let newConfig = {};
        if (config.name != this.name) {
          newConfig.name = this.name;
          config.name = this.name;
        }
        if (this.password != "") {
          newConfig.password = this.password;
        }
        if (config.wifi_ssid != this.wifi_ssid) {
          newConfig.wifi_ssid = this.wifi_ssid;
          config.wifi_ssid = this.wifi_ssid;
        }
        if (config.wifi_pass != this.wifi_pass) {
          newConfig.wifi_pass = this.wifi_pass;
          config.wifi_pass = this.wifi_pass;
        }
        // not sure if we want people to manually change the AES key just yet
        // but in either case it will need proper validation
        // if (config.aes_key != this.aes_key) {
        //   newConfig.aes_key = this.aes_key;
        // }
        try {
          const response = await axios.post("/config", newConfig, {
            withCredentials: true,
          });
          if (response.data == "OK") {
            this.config = config;
            this.savingSuccess = true;
            setTimeout(() => {
              // TODO: this still needs some work as on a name change https certificate needs
              // to be regenerated and that takes quite some time.
              // Maybe we can test with some ajax calls from time to time and only reload when
              // ESP GW becomes available (try http only to bypass cert error)
              window.location.href = "https://" + config.name + ".local";
            }, 2000);
          } else {
            this.errors.push("Error saving configuration data");
          }
        } catch (error) {
          if (typeof error.toString != "undefined") {
            this.errors.push(error.toString());
          } else {
            console.log(error);
            this.errors.push("Error saving configuration data");
          }
        }
        this.saving = false;
      }
    },
    toggleShowPassword() {
      if (this.attrPasswordType == "password") {
        this.attrPasswordType = "text";
      } else {
        this.attrPasswordType = "password";
      }
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
<style scoped>
.icon {
  width: 1.2em;
  display: inline-block;
  text-transform: none;
  line-height: 1;
  vertical-align: text-bottom;
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  filter: invert(1);
}
</style>