import os
# Function to generate 20 random IP destination addresses using os.urandom
def generate_random_ip_addresses(num_addresses=100000):
    ip_addresses = []
    for _ in range(num_addresses):
        # Use os.urandom to generate random bytes (4 bytes for an IP address)
        random_bytes = os.urandom(4)
        # Convert bytes to integer and create the IP address
        ip_address = '.'.join(str(byte) for byte in random_bytes)
        ip_addresses.append(ip_address)
    return ip_addresses

# Function to convert IP addresses to hexadecimal format
def convert_ip_to_hex(ip_addresses):
    hex_addresses = []
    for ip in ip_addresses:
        # Convert each part of the IP address to hexadecimal
        hex_ip = ''.join([format(int(part), '02X') for part in ip.split('.')])
        hex_addresses.append(hex_ip)
    return hex_addresses

# Generate random IP addresses using os.urandom
random_ip_addresses = generate_random_ip_addresses()

# Convert to hexadecimal format
hex_ip_addresses = convert_ip_to_hex(random_ip_addresses)

# Save the hexadecimal IP addresses to a text file
with open("addresses-100000.txt", "w") as file:
    for ip in hex_ip_addresses:
        file.write(ip + "\n")

print("Hexadecimal IP addresses have been saved to 'addresses-100000.txt'.")
