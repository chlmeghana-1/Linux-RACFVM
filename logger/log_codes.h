// logger/log_codes.h
#ifndef LOG_CODES_H
#define LOG_CODES_H

// Base IDs (macro maps to the help text)
#define IUCVCLNT002 "Processing user command"
#define IUCVCLNT010 "Unable to create socket"
#define IUCVCLNT011 "Socket created"
#define IUCVCLNT012 "Socket connection to the server is success"
#define IUCVCLNT013 "Server might be up but not ready"
#define IUCVCLNT014 "Server is down or not listening"
#define IUCVCLNT015 "Failed to connect to the server"
#define IUCVCLNT016 "Server response timeout"
#define IUCVCLNT017 "Server connection closed"
#define IUCVCLNT018 "Invalid response status"
#define IUCVCLNT019 "Unknown tag in the server response"
#define IUCVCLNT020 "Successfully sent the command to RACF"
#define IUCVCLNT021 "Failed to send the command to RACF"
#define IUCVCLNT022 "Response is received from the server with error."
#define IUCVCLNT023 "Parsed the server response"
#define IUCVCLNT024 "Parsed the response from server successfully"
#define IUCVCLNT025 "Failed to set timeout to the socket"
#define IUCVCLNT026 "Failed to send the command to server"

// Add more codes here as needed…

#endif // LOG_CODES_H
