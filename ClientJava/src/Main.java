import java.io.*;
import java.net.*;
import java.util.Scanner;

public class Main {
    public static void main(String[] args) {
        try {
            Socket socket = new Socket("localhost", 12345);
            Client client = new Client(socket);
            Scanner scanner = new Scanner(System.in);

            System.out.print("Enter client name (max length is 988 symbols): ");
            String name = scanner.nextLine();
            client.sendMessage("Hello, server! How are you? This is " + name);
            client.outputServerResponse();

            String userInput = scanner.nextLine();
            while(true){
                if (socket.isClosed()) {
                    System.out.println("\033[91mServer has terminated the connection. Exiting...\033[0m");
                    break;
                }
                if(userInput.trim().equalsIgnoreCase("EXIT")){
                    client.sendMessage("\033[91mClient decided to terminate the connection.\033[0m");
                    break;
                }
                if(userInput.isBlank() || Request.isInvalidRequest(userInput.trim()))System.out.println("\033[91mUndefined request.\033[0m");
                else if (userInput.trim().length() > 1024)System.out.println("The message length exceeds 1024 bytes, which is the maximum.");
                else client.processRequest(userInput.trim());
                userInput = scanner.nextLine();
            }
            socket.close();
            scanner.close();
        } catch (ConnectException e) {
            System.out.println("Connection could not be established. Ensure the server is running.");
        } catch (IOException e) {
            System.out.println("I/O error: " + e.getMessage());
        }
    }
}