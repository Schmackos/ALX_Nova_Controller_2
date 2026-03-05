        // HAL Device YAML Parser
        // Flat key-value YAML parser for ALX Nova HAL device descriptors

        function parseDeviceYaml(text) {
            var result = {};
            if (!text) return result;
            var lines = text.split('\n');
            for (var i = 0; i < lines.length; i++) {
                var line = lines[i].trim();
                if (!line || line.charAt(0) === '#') continue;
                var colonIdx = line.indexOf(':');
                if (colonIdx < 1) continue;
                var key = line.substring(0, colonIdx).trim();
                var val = line.substring(colonIdx + 1).trim();
                // Strip surrounding quotes
                if (val.length >= 2 && val.charAt(0) === '"' && val.charAt(val.length - 1) === '"') {
                    val = val.substring(1, val.length - 1);
                }
                result[key] = val;
            }
            return result;
        }

        function deviceToYaml(obj) {
            var lines = [];
            lines.push('hal_version: 1');
            if (obj.compatible) lines.push('compatible: "' + obj.compatible + '"');
            if (obj.name) lines.push('name: "' + obj.name + '"');
            if (obj.manufacturer) lines.push('manufacturer: "' + obj.manufacturer + '"');
            var typeNames = {1: 'DAC', 2: 'ADC', 3: 'CODEC', 4: 'AMP', 5: 'DSP', 6: 'SENSOR'};
            if (obj.type && typeNames[obj.type]) lines.push('device_type: ' + typeNames[obj.type]);
            if (obj.i2cAddr !== undefined) lines.push('i2c_default_address: 0x' + obj.i2cAddr.toString(16).toUpperCase().padStart(2, '0'));
            if (obj.channels) lines.push('channel_count: ' + obj.channels);
            return lines.join('\n') + '\n';
        }

        function importDeviceYaml() {
            var input = document.createElement('input');
            input.type = 'file';
            input.accept = '.yaml,.yml';
            input.onchange = function(e) {
                var file = e.target.files[0];
                if (!file) return;
                var reader = new FileReader();
                reader.onload = function(ev) {
                    var parsed = parseDeviceYaml(ev.target.result);
                    if (parsed.compatible) {
                        showToast('Imported: ' + (parsed.name || parsed.compatible), 'success');
                    } else {
                        showToast('YAML missing required "compatible" field', 'error');
                    }
                };
                reader.readAsText(file);
            };
            input.click();
        }
