import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

# === Load datasets ===
df1 = pd.read_csv("/home/alessandro/zalf-rpm/monica/output_standard.csv", skiprows=[0, 2], header=0)
df1 = df1.iloc[:2540]
df1['Date'] = pd.to_datetime(df1['Date'], format='%Y-%m-%d', errors='coerce')
df1 = df1.dropna(subset=['Date'])

df2 = pd.read_csv("/home/alessandro/zalf-rpm/monica/output_monocrop.csv", skiprows=[0, 2], header=0)
df2 = df2.iloc[:2540]
df2['Date'] = pd.to_datetime(df2['Date'], format='%Y-%m-%d', errors='coerce')
df2 = df2.dropna(subset=['Date'])

# === Filter date window ===
start_date = "1991-08-01"
end_date = "1997-08-31"
df1 = df1[(df1['Date'] >= start_date) & (df1['Date'] <= end_date)]
df2 = df2[(df2['Date'] >= start_date) & (df2['Date'] <= end_date)]

# Columns that SHOULD be numeric (adjust as needed)
numeric_cols = ['Yield', 'Evapotranspiration', 'AbBiom', 'Stage', 'Evaporation',
                'Tra', 'Act_ET', 'Mois_1', 'Mois_2', 'Mois_3',
                'N_1', 'N_2', 'N_3', 'rootDensity_1', 'rootDensity_2', 'rootDensity_3']

# Convert to numeric in both dataframes
for col in numeric_cols:
    if col in df1.columns:
        df1[col] = pd.to_numeric(df1[col], errors='coerce')
    if col in df2.columns:
        df2[col] = pd.to_numeric(df2[col], errors='coerce')

# === Define variable groups ===
scalar_vars = ['Stage', 'Evaporation', 'Evapotranspiration', 'Tra', 'Act_ET', 'Yield']
layered_var_groups = {
    'Mois': ['Mois_1', 'Mois_2', 'Mois_3'],
    'N': ['N_1', 'N_2', 'N_3'],
    'rootDensity': ['rootDensity_1', 'rootDensity_2', 'rootDensity_3']
}
variables = scalar_vars + list(layered_var_groups.keys())  # 7 scalars + 3 grouped = 10 plots

# === Styling ===
dataset_colors = {
    'df1': 'blue',
    'df2': 'red'
}

scalar_linestyles = {
    'df1': 'solid',
    'df2': 'dashed'
}

layer_styles = {
    1: {'linestyle': 'solid', 'label': 'Layer 1'},
    2: {'linestyle': 'dashed', 'label': 'Layer 2'},
    3: {'linestyle': 'dotted', 'label': 'Layer 3'}
}

# === Plotting ===
fig, axes = plt.subplots(3, 3, figsize=(18, 15), sharex=True)
axes = axes.flatten()

for i, var in enumerate(variables):
    ax = axes[i]

    # --- Scalar variables ---
    if var in scalar_vars:
        if var in df1.columns:
            ax.plot(df1['Date'], df1[var],
                    color=dataset_colors['df1'],
                    linestyle=scalar_linestyles['df1'],
                    label=f'{var} (MONICOSMO)')

        if var in df2.columns:
            ax.plot(df2['Date'], df2[var],
                    color=dataset_colors['df2'],
                    linestyle=scalar_linestyles['df2'],
                    label=f'{var} (original)')

    # --- Layered variables ---
    elif var in layered_var_groups:
        for j, layer_var in enumerate(layered_var_groups[var], start=1):
            style = layer_styles[j]

            if layer_var in df1.columns:
                ax.plot(df1['Date'], df1[layer_var],
                        color=dataset_colors['df1'],
                        linestyle=style['linestyle'],
                        label=f'{var} L{j} (MONICOSMO)')

            if layer_var in df2.columns:
                ax.plot(df2['Date'], df2[layer_var],
                        color=dataset_colors['df2'],
                        linestyle=style['linestyle'],
                        label=f'{var} L{j} (original)')

    else:
        ax.text(0.5, 0.5, f"{var} not found", ha='center', va='center')

    ax.set_title(var)
    ax.grid(True)
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%b-%Y'))
    ax.tick_params(axis='x', rotation=45)

    # Clean duplicate legend entries
    handles, labels = ax.get_legend_handles_labels()
    by_label = dict(zip(labels, handles))
    ax.legend(by_label.values(), by_label.keys(), fontsize='small')

# Hide empty subplots if any
for j in range(i+1, len(axes)):
    axes[j].axis('off')

# === Final layout ===
plt.suptitle("Comparison: Dataset 1 vs. Dataset 2 (Daily Values)", fontsize=16)
plt.tight_layout(rect=[0, 0, 1, 0.97])
plt.show()