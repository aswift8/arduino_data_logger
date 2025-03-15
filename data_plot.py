import data_util
import matplotlib.pyplot as plt


if __name__ == "__main__":
    data = data_util.read_data("data/20250314-233630-dat2.csv")
    ts = data["timestamp"]
    fig, ax = plt.subplots(4, 1)
    la, = ax[0].plot(ts, data["analog"])
    la.set_label("voltage")
    ax[0].set_title("Analog input (V)")
    ax[0].legend()
    lb0, = ax[1].plot(ts, data["btn_0"])
    lb1, = ax[1].plot(ts, data["btn_1"])
    lb0.set_label("Button 0")
    lb1.set_label("Button 1")
    ax[1].set_title("Buttons")
    ax[1].legend()
    lax, = ax[2].plot(ts, data["acc"][:,0])
    lay, = ax[2].plot(ts, data["acc"][:,1])
    laz, = ax[2].plot(ts, data["acc"][:,2])
    lax.set_label("x")
    lay.set_label("y")
    laz.set_label("z")
    ax[2].set_title("Accelerometer (m/s²)")
    ax[2].legend()
    lgx, = ax[3].plot(ts, data["gyro"][:,0])
    lgy, = ax[3].plot(ts, data["gyro"][:,1])
    lgz, = ax[3].plot(ts, data["gyro"][:,2])
    lgx.set_label("x")
    lgy.set_label("y")
    lgz.set_label("z")
    ax[3].set_title("Gyro (rad/s)")
    ax[3].legend()
    plt.tight_layout(pad=1.0)
    plt.show()

