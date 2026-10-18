import re, serial, matplotlib.pyplot as plt

# text = "9999999999[LOG][I][MT6701_1]: iA:  171.83 pA:  171.78 D:   0.05 V:  -0.04 aV:  -0.02 rV:  -0.48 sV:  -0.02"

# Pattern updated with -? to catch optional negative signs
# prefix = re.compile(r"\s*\d+\[[\w_ ]+\]\[[\w_ ]+\]\[[\w_ ]+\]: (.*)")
prefix = re.compile(r"\s*\d+\[[\w_ ]+\]\[[\w_ ]+\]\[MT6701_1\]: (.*)")
pattern = re.compile(r"([A-Za-z0-9_ ]+):\s*(-?[\d.]+)")

ser = serial.Serial("COM13", 115200, timeout=1)

data = {}

plt.ion()
fig, ax = plt.subplots()

while True:
    line = ser.readline().decode("utf-8").strip()
    re_obj = re.search(prefix, line)
    if not re_obj:
        continue
    text = re_obj.group(1)

    pairs = re.findall(pattern, text)
    kv_dict = {key.strip(): float(val) for key, val in pairs}

    for key, val in kv_dict.items():
        if key not in data:
            data[key] = [val]
        else:
            data[key].append(val)
            if len(data[key]) > 50:
                data[key].pop(0)


    ax.cla()
    for key, vals in data.items():
        ax.plot(vals, label=key)

    ax.legend(loc="upper left")
    plt.ylim(-500,500)
    plt.pause(0.000001)
    ser.reset_input_buffer()
